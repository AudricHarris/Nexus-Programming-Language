#!/usr/bin/env node

const {
  createConnection,
  TextDocuments,
  ProposedFeatures,
  DidChangeConfigurationNotification,
  CompletionItemKind,
  TextDocumentSyncKind,
  InsertTextFormat,
} = require('vscode-languageserver/node');

const { TextDocument } = require('vscode-languageserver-textdocument');

const connection = createConnection(ProposedFeatures.all);
const documents  = new TextDocuments(TextDocument);

// ─────────────────────────────────────────────

const KEYWORDS = [
  'if', 'else', 'for', 'while', 'return', 'match', 'new',
  'export', 'import', 'static', 'self', 'Sequential',
  'true', 'false', 'public', 'private', 'protected',
  'const', 'sum', 'class', 'implement', 'Constructor', 'Factory',
];

const TYPES = [
  'i8', 'i16', 'i32', 'i64',
  'u8', 'u16', 'u32', 'u64',
  'f32', 'f64',
  'bool', 'char', 'str', 'string', 'void',
  'let',
];

// Rich built-in function table: name -> { sig, doc }
const BUILTIN_FUNCTIONS = {
  'Print':         { sig: 'Print(value: any)',                      doc: 'Print a value to stdout without a newline.' },
  'Printf':        { sig: 'Printf(fmt: str, ...args)',               doc: 'Formatted print. Use {var} for interpolation and \\n for newlines. Supports #RRGGBB colour prefix.' },
  'Warn':          { sig: 'Warn(value: any)',                        doc: 'Print a warning to stderr.' },
  'Warnf':         { sig: 'Warnf(fmt: str, ...args)',                doc: 'Formatted warning to stderr.' },
  'Read':          { sig: 'Read() -> str',                          doc: 'Read a line of input from stdin. Returns trimmed string.' },
  'Random':        { sig: 'Random() -> f64',                         doc: 'Return a random f64 in [0, 1).' },
};

// Snippets: label -> { insert, doc }
const SNIPPETS = {
  'class': {
    insert: 'class ${1:ClassName}\n{\n\t$0\n}',
    doc: 'Class declaration',
  },
  'sum': {
    insert: 'sum ${1:TypeName}\n{\n\t${2:Variant}\n}',
    doc: 'Sum type (enum) declaration',
  },
  'match': {
    insert: 'match (${1:value})\n{\n\t${2:Pattern} => ${3:expression}\n}',
    doc: 'Pattern match',
  },
  'while': {
    insert: 'while (${1:condition})\n{\n\t$0\n}',
    doc: 'While loop',
  },
  'if': {
    insert: 'if (${1:condition})\n{\n\t$0\n}',
    doc: 'If statement',
  },
  'ifelse': {
    insert: 'if (${1:condition})\n{\n\t${2}\n} else {\n\t$0\n}',
    doc: 'If / else statement',
  },
  'fn': {
    insert: '${1:FunctionName}(${2:params}) -> ${3:void}\n{\n\t$0\n}',
    doc: 'Function definition',
  },
  'Options<T>': {
    insert: 'Options<${1:T}>',
    doc: 'Generic Options type',
  },
};

const workspaceSymbols  = new Map();
const documentToWorkspace = new Map();

function getSymbolStore(wsKey) {
  const key = wsKey || '__global__';
  if (!workspaceSymbols.has(key)) workspaceSymbols.set(key, new Map());
  return workspaceSymbols.get(key);
}

function resolveWorkspace(docUri) {
  return documentToWorkspace.get(docUri) || '__global__';
}

let hasConfigurationCapability   = false;
let hasWorkspaceFolderCapability = false;

const SEMANTIC_TOKEN_TYPES = [
  'keyword',     // 0
  'type',        // 1
  'function',    // 2
  'variable',    // 3
  'number',      // 4
  'string',      // 5
  'comment',     // 6
  'operator',    // 7
  'punctuation', // 8
];

connection.onInitialize((params) => {
  const caps = params.capabilities;

  hasConfigurationCapability   = !!(caps.workspace && caps.workspace.configuration);
  hasWorkspaceFolderCapability = !!(caps.workspace && caps.workspace.workspaceFolders);

  if (params.workspaceFolders) {
    params.workspaceFolders.forEach(f => workspaceSymbols.set(f.uri, new Map()));
  }

  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,

      completionProvider: {
        resolveProvider: true,
        triggerCharacters: ['.', ':'],
      },

      semanticTokensProvider: {
        legend: { tokenTypes: SEMANTIC_TOKEN_TYPES, tokenModifiers: [] },
        full: true,
      },

      // Auto-close /! comment — fires when user types '!'
      documentOnTypeFormattingProvider: {
        firstTriggerCharacter: '!',
        moreTriggerCharacter:  [],
      },

      hoverProvider: true,

      workspaceSymbolProvider: true,

      ...(hasWorkspaceFolderCapability
        ? { workspace: { workspaceFolders: { supported: true } } }
        : {}),
    },
  };
});

connection.onInitialized(() => {
  if (hasConfigurationCapability) {
    connection.client.register(DidChangeConfigurationNotification.type, undefined);
  }
  if (hasWorkspaceFolderCapability) {
    connection.workspace.onDidChangeWorkspaceFolders(event => {
      event.added.forEach(f => {
        if (!workspaceSymbols.has(f.uri)) workspaceSymbols.set(f.uri, new Map());
      });
      event.removed.forEach(f => workspaceSymbols.delete(f.uri));
    });
  }
});

function extractSymbols(text, docUri) {
  const wsKey = resolveWorkspace(docUri);
  const store  = getSymbolStore(wsKey);

  // Clear stale entries from this file
  for (const [name, info] of store.entries()) {
    if (info.uri === docUri) store.delete(name);
  }

  let m;

  // Top-level function:  Name(params) -> RetType {
  const fnRx = /^([A-Z][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([a-zA-Z0-9_\[\]]+))?\s*\{/gm;
  while ((m = fnRx.exec(text)) !== null) {
    const name   = m[1];
    const params = m[2].trim();
    const ret    = m[3] ? m[3].trim() : 'void';
    store.set(name, {
      kind:   CompletionItemKind.Function,
      detail: `${name}(${params}) -> ${ret}`,
      doc:    'User-defined function',
      uri:    docUri,
    });
  }

  // class Name
  const classRx = /\bclass\s+([A-Z][a-zA-Z0-9_]*)/g;
  while ((m = classRx.exec(text)) !== null) {
    store.set(m[1], { kind: CompletionItemKind.Class, detail: `class ${m[1]}`, doc: 'User-defined class', uri: docUri });
  }

  // sum Name
  const sumRx = /\bsum\s+([A-Z][a-zA-Z0-9_]*)/g;
  while ((m = sumRx.exec(text)) !== null) {
    store.set(m[1], { kind: CompletionItemKind.Enum, detail: `sum ${m[1]}`, doc: 'Sum type', uri: docUri });
  }

  // Local / typed variables: i32 name, bool name, let name, etc.
  const typePattern = TYPES.join('|');
  const varRx = new RegExp(`\\b(?:${typePattern})(?:\\[\\])*\\s+([a-z_][a-zA-Z0-9_]*)`, 'g');
  while ((m = varRx.exec(text)) !== null) {
    const name = m[1];
    if (!store.has(name)) {
      store.set(name, { kind: CompletionItemKind.Variable, detail: `variable ${name}`, doc: 'Local variable', uri: docUri });
    }
  }
}

async function mapDocumentToWorkspace(docUri) {
  if (documentToWorkspace.has(docUri) || !hasWorkspaceFolderCapability) return;
  try {
    const folders = await connection.workspace.getWorkspaceFolders();
    if (!folders) return;
    let best = null;
    for (const f of folders) {
      if (docUri.startsWith(f.uri) && (!best || f.uri.length > best.uri.length)) best = f;
    }
    if (best) documentToWorkspace.set(docUri, best.uri);
  } catch (_) {}
}

documents.onDidChangeContent(async change => {
  const doc = change.document;
  await mapDocumentToWorkspace(doc.uri);
  extractSymbols(doc.getText(), doc.uri);
  validateTextDocument(doc);
});

documents.onDidOpen(async event => {
  const doc = event.document;
  await mapDocumentToWorkspace(doc.uri);
  extractSymbols(doc.getText(), doc.uri);
});

documents.onDidClose(event => {
  // Intentionally keep symbols in memory across close — that is the "remember" feature.
  connection.sendDiagnostics({ uri: event.document.uri, diagnostics: [] });
});

connection.onDocumentOnTypeFormatting((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;

  const text   = doc.getText();
  const offset = doc.offsetAt(params.position);

  // Need at least 2 chars before cursor
  if (offset < 2) return null;

  const before2 = text.slice(offset - 2, offset);
  if (before2 !== '/!') return null;

  const textBefore = text.slice(0, offset - 2);
  const opens  = (textBefore.match(/\/!/g) || []).length;
  const closes = (textBefore.match(/!\//g) || []).length;
  if (opens > closes) return null;

  return [{
    range:   { start: params.position, end: params.position },
    newText: '  !/',
  }];
});

connection.onHover((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;

  const text   = doc.getText();
  const offset = doc.offsetAt(params.position);

  // Expand selection to full word (allow dots for Math.Sqrt etc.)
  let start = offset;
  let end   = offset;
  while (start > 0 && /[a-zA-Z0-9_.]/.test(text[start - 1])) start--;
  while (end < text.length && /[a-zA-Z0-9_.]/.test(text[end])) end++;
  const word = text.slice(start, end);

  if (BUILTIN_FUNCTIONS[word]) {
    const { sig, doc: d } = BUILTIN_FUNCTIONS[word];
    return { contents: { kind: 'markdown', value: `\`\`\`nexus\n${sig}\n\`\`\`\n\n${d}` } };
  }

  const store = getSymbolStore(resolveWorkspace(doc.uri));
  if (store.has(word)) {
    const sym = store.get(word);
    return { contents: { kind: 'markdown', value: `\`\`\`nexus\n${sym.detail}\n\`\`\`\n\n${sym.doc}` } };
  }

  return null;
});

connection.onWorkspaceSymbol((params) => {
  const query   = params.query.toLowerCase();
  const results = [];

  for (const [, store] of workspaceSymbols) {
    for (const [name, info] of store) {
      if (!query || name.toLowerCase().includes(query)) {
        results.push({
          name,
          kind:     info.kind,
          location: {
            uri:   info.uri,
            range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
          },
        });
      }
    }
  }

  return results;
});

connection.onCompletion((params) => {
  const doc = documents.get(params.textDocument.uri);
  const completions = [];

  // Keywords
  KEYWORDS.forEach(kw => completions.push({
    label: kw,
    kind:  CompletionItemKind.Keyword,
    data:  { type: 'keyword', name: kw },
  }));

  // Types
  TYPES.forEach(t => completions.push({
    label: t,
    kind:  CompletionItemKind.TypeParameter,
    data:  { type: 'builtin_type', name: t },
  }));

  // Built-in functions
  Object.keys(BUILTIN_FUNCTIONS).forEach(fn => completions.push({
    label:  fn,
    kind:   CompletionItemKind.Function,
    detail: BUILTIN_FUNCTIONS[fn].sig,
    data:   { type: 'builtin_fn', name: fn },
  }));

  // Snippets
  Object.entries(SNIPPETS).forEach(([label, snip]) => completions.push({
    label,
    kind:             CompletionItemKind.Snippet,
    insertText:       snip.insert,
    insertTextFormat: InsertTextFormat.Snippet,
    detail:           snip.doc,
    data:             { type: 'snippet', name: label },
  }));

  // User-defined symbols from workspace memory
  if (doc) {
    const store = getSymbolStore(resolveWorkspace(doc.uri));
    for (const [name, info] of store) {
      if (BUILTIN_FUNCTIONS[name] || KEYWORDS.includes(name) || TYPES.includes(name)) continue;
      completions.push({
        label:  name,
        kind:   info.kind,
        detail: info.detail,
        data:   { type: 'user_symbol', name },
      });
    }
  }

  return completions;
});

connection.onCompletionResolve((item) => {
  const d = item.data;
  if (!d) return item;

  if (d.type === 'keyword') {
    item.detail = 'Nexus keyword';
    item.documentation = { kind: 'plaintext', value: `Keyword: ${d.name}` };
  } else if (d.type === 'builtin_type') {
    item.detail = 'Nexus built-in type';
    item.documentation = { kind: 'plaintext', value: `Built-in type: ${d.name}` };
  } else if (d.type === 'builtin_fn') {
    const fn = BUILTIN_FUNCTIONS[d.name];
    if (fn) {
      item.detail = fn.sig;
      item.documentation = { kind: 'markdown', value: fn.doc };
    }
  } else if (d.type === 'snippet') {
    const sn = SNIPPETS[d.name];
    if (sn) item.documentation = { kind: 'plaintext', value: sn.doc };
  } else if (d.type === 'user_symbol') {
    item.documentation = { kind: 'plaintext', value: 'Defined in workspace' };
  }

  return item;
});

function validateTextDocument(textDocument) {
  const text        = textDocument.getText();
  const diagnostics = [];

  // Compiler directives  /![NAME]!/
  const directives  = new Set([...text.matchAll(/\/!\[([A-Z_]+)\]!\//g)].map(m => m[1]));
  const STRICT      = directives.has('STRICT');
  const WARN_UNUSED = directives.has('WARN_UNUSED');
  const NO_GLOBALS  = directives.has('NO_GLOBALS');
  const NO_WARNINGS = directives.has('NO_WARNINGS');

  const openBraces  = (text.match(/\{/g) || []).length;
  const closeBraces = (text.match(/\}/g) || []).length;
  if (openBraces !== closeBraces) {
    diagnostics.push({
      severity: 1,
      range:    { start: textDocument.positionAt(0), end: textDocument.positionAt(text.length) },
      message:  `Unmatched braces: ${openBraces} opening vs ${closeBraces} closing`,
      source:   'nexus-lsp',
    });
  }

  const openComments  = (text.match(/\/!/g) || []).length;
  const closeComments = (text.match(/!\//g) || []).length;
  if (openComments !== closeComments) {
    diagnostics.push({
      severity: 2,
      range:    { start: textDocument.positionAt(0), end: textDocument.positionAt(text.length) },
      message:  `Unclosed block comment: ${openComments} /!  but ${closeComments} !/`,
      source:   'nexus-lsp',
    });
  }

  if (STRICT) {
    const rx = /\blet\b/g;
    let m;
    while ((m = rx.exec(text)) !== null) {
      diagnostics.push({
        severity: 1,
        range:    { start: textDocument.positionAt(m.index), end: textDocument.positionAt(m.index + 3) },
        message:  '"let" is forbidden in STRICT mode. Use an explicit type.',
        source:   'nexus-lsp',
      });
    }
  }

  if (NO_GLOBALS) {
    const typePattern = [...TYPES, 'let'].join('|');
    const rx = new RegExp(`^\\s*(?:${typePattern})(?:\\[\\])*\\s+([a-zA-Z_][a-zA-Z0-9_]*)`, 'gm');
    let m;
    while ((m = rx.exec(text)) !== null) {
      const before = text.slice(0, m.index);
      const open   = (before.match(/\{/g) || []).length;
      const close  = (before.match(/\}/g) || []).length;
      if (open === close) {
        diagnostics.push({
          severity: 1,
          range:    { start: textDocument.positionAt(m.index), end: textDocument.positionAt(m.index + m[0].length) },
          message:  'Global variables are not allowed (NO_GLOBALS)',
          source:   'nexus-lsp',
        });
      }
    }
  }

  if (WARN_UNUSED && !NO_WARNINGS) {
    const typePattern = [...TYPES, 'let'].join('|');
    const rx = new RegExp(`\\b(?:${typePattern})(?:\\[\\])*\\s+([a-zA-Z_][a-zA-Z0-9_]*)`, 'g');
    let m;
    while ((m = rx.exec(text)) !== null) {
      const name  = m[1];
      const count = [...text.matchAll(new RegExp(`\\b${name}\\b`, 'g'))].length;
      if (count <= 1) {
        diagnostics.push({
          severity: 2,
          range:    { start: textDocument.positionAt(m.index), end: textDocument.positionAt(m.index + m[0].length) },
          message:  `Variable '${name}' is declared but never used`,
          source:   'nexus-lsp',
        });
      }
    }
  }

  {
    const fnRx = /([A-Z][a-zA-Z0-9_]*)\s*\([^)]*\)\s*->\s*void\s*\{/g;
    let fm;
    while ((fm = fnRx.exec(text)) !== null) {
      let depth = 0, i = fm.index + fm[0].length - 1;
      const bodyStart = i;
      while (i < text.length) {
        if (text[i] === '{') depth++;
        else if (text[i] === '}') { depth--; if (depth === 0) break; }
        i++;
      }
      const body  = text.slice(bodyStart, i);
      const retRx = /\breturn\s+(?!;)(\S)/g;
      let rm;
      while ((rm = retRx.exec(body)) !== null) {
        const absIdx = bodyStart + rm.index;
        diagnostics.push({
          severity: 2,
          range:    { start: textDocument.positionAt(absIdx), end: textDocument.positionAt(absIdx + rm[0].length) },
          message:  `Function '${fm[1]}' is declared -> void but returns a value`,
          source:   'nexus-lsp',
        });
      }
    }
  }

  const final = NO_WARNINGS
    ? diagnostics.filter(d => d.severity === 1)
    : diagnostics;

  connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: final });
}

connection.languages.semanticTokens.on((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return { data: [] };

  const text = doc.getText();
  const data = [];
  let pos = 0, line = 0, col = 0, lastLine = 0, lastCol = 0;

  const pushToken = (tLine, tCol, length, tType) => {
    const lineDelta = tLine - lastLine;
    const colDelta  = lineDelta === 0 ? tCol - lastCol : tCol;
    data.push(lineDelta, colDelta, length, tType, 0);
    lastLine = tLine;
    lastCol  = tCol;
  };

  const syncPos = (from, to) => {
    for (let i = from; i < to; i++) {
      if (text[i] === '\n') { line++; col = 0; } else { col++; }
    }
  };

  while (pos < text.length) {
    const ch = text[pos];

    if (ch === '\n') { line++; col = 0; pos++; continue; }
    if (/\s/.test(ch)) { col++; pos++; continue; }

    // /* single-line comment (ends at newline) */
    if (text.startsWith('/*', pos)) {
      const sl = line, sc = col;
      let end = text.indexOf('\n', pos);
      if (end === -1) end = text.length;
      pushToken(sl, sc, end - pos, 6);
      col += end - pos;
      pos = end;
      continue;
    }

    // /! block comment !/  (multi-line)
    if (text.startsWith('/!', pos)) {
      const sl = line, sc = col;
      let end = text.indexOf('!/', pos + 2);
      if (end === -1) end = text.length; else end += 2;
      const seg   = text.slice(pos, end).split('\n');
      if (seg.length === 1) {
        pushToken(sl, sc, seg[0].length, 6);
        col += seg[0].length;
      } else {
        for (let i = 0; i < seg.length; i++) {
          if (seg[i].length > 0) pushToken(sl + i, i === 0 ? sc : 0, seg[i].length, 6);
        }
        line = sl + seg.length - 1;
        col  = seg[seg.length - 1].length;
      }
      pos = end;
      continue;
    }

    // char literal  'x'  or  '\n'
    if (ch === "'") {
      const sl = line, sc = col;
      let end = pos + 1;
      if (end < text.length && text[end] === '\\') end += 2; else end++;
      if (end < text.length && text[end] === "'") end++;
      pushToken(sl, sc, end - pos, 5);
      col += end - pos;
      pos = end;
      continue;
    }

    // string literal  "..."
    if (ch === '"') {
      const sl = line, sc = col, start = pos;
      let end = pos + 1;
      while (end < text.length && !(text[end] === '"' && text[end - 1] !== '\\')) end++;
      if (end < text.length) end++;
      pushToken(sl, sc, end - start, 5);
      syncPos(start, end);
      pos = end;
      continue;
    }

    // number
    if (/[0-9]/.test(ch)) {
      const sl = line, sc = col, start = pos;
      while (pos < text.length && /[0-9.eE+\-]/.test(text[pos])) pos++;
      pushToken(sl, sc, pos - start, 4);
      col += pos - start;
      continue;
    }

    // identifier / keyword / type / builtin
    if (/[a-zA-Z_]/.test(ch)) {
      const sl = line, sc = col, start = pos;
      while (pos < text.length && /[a-zA-Z0-9_.]/.test(text[pos])) pos++;
      const word = text.slice(start, pos);

      let tt;
      if (KEYWORDS.includes(word))       tt = 0; // keyword
      else if (TYPES.includes(word))     tt = 1; // type
      else if (/^[A-Z]/.test(word))      tt = 1; // PascalCase = type/class
      else if (BUILTIN_FUNCTIONS[word])  tt = 2; // built-in fn
      else                               tt = 3; // variable

      pushToken(sl, sc, word.length, tt);
      col += word.length;
      continue;
    }

    // operator
    if ('+-*/%=<>!&|^~'.includes(ch)) {
      pushToken(line, col, 1, 7);
      col++; pos++;
      continue;
    }

    // punctuation
    if ('(){}[],.;:'.includes(ch)) {
      pushToken(line, col, 1, 8);
      col++; pos++;
      continue;
    }

    col++; pos++;
  }

  return { data };
});

documents.listen(connection);
connection.listen();
connection.console.log('Nexus LSP Server started');
