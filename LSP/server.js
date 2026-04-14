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
const documents = new TextDocuments(TextDocument);

// ─────────────────────────────────────────────

const KEYWORDS = [
  'if', 'else', 'for', 'while', 'range', 'return', 'match', 'new',
  'export', 'import', 'static', 'self', 'Sequential',
  'true', 'false', 'public', 'private', 'protected',
  'const', 'class', 'implement', 'impl', 'struct', 'extern',
  'embed', 'fn', 'mut', 'enum', 'sum', 'let', 'trait'
];

const TYPES = [
  'i8', 'i16', 'i32', 'i64', 'int', 'long', 'short',
  'u8', 'u16', 'u32', 'u64', 'uint', 'ulong', 'ushort',
  'f32', 'f64', 'float',
  'bool', 'char', 'str', 'string', 'void', 'ptr',
  'Vec2', 'Vec3', 'Vec4', 'Option', 'Array',
];

// Rich built-in function table: name -> { sig, doc }
const BUILTIN_FUNCTIONS = {
  'Print':    { sig: 'Print(value: any)',            doc: 'Print a value to stdout without a newline.' },
  'Printf':   { sig: 'Printf(fmt: str, ...args)',    doc: 'Formatted print. Use {var} for interpolation and \\n for newlines.' },
  'Warn':     { sig: 'Warn(value: any)',             doc: 'Print a warning to stderr.' },
  'Warnf':   { sig: 'Warnf(fmt: str, ...args)',     doc: 'Formatted warning to stderr.' },
  'Read':     { sig: 'Read() -> str',                doc: 'Read a line of input from stdin. Returns trimmed string.' },
  'Random':   { sig: 'Random() -> f64',              doc: 'Return a random f64 in [0, 1).' },
  'RandomInt':{ sig: 'RandomInt(min: i32, max: i32) -> i32', doc: 'Return a random integer in [min, max].' },
  'print':    { sig: 'print(value: any)',            doc: 'Print a value to stdout.' },
};

// Snippets: label -> { insert, doc }
const SNIPPETS = {
  'class': {
    insert: 'class ${1:ClassName}\n{\n\t$0\n}',
    doc: 'Class declaration',
  },
  'struct': {
    insert: 'struct ${1:Name}\n{\n\t${2:field}: ${3:type};\n\t$0\n}',
    doc: 'Struct declaration',
  },
  'enum': {
    insert: 'enum ${1:TypeName}\n{\n\t${2:Variant}\n}',
    doc: 'Enum declaration',
  },
  'sum': {
    insert: 'sum ${1:TypeName}\n{\n\t${2:Variant}\n}',
    doc: 'Sum type declaration',
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
    insert: 'fn ${1:name}(${2:params}) -> ${3:void}\n{\n\t$0\n}',
    doc: 'Function definition',
  },
  'impl': {
    insert: 'impl ${1:TraitName}\n{\n\t$0\n}',
    doc: 'Impl block',
  },
  'Option<T>': {
    insert: 'Option<${1:T}>',
    doc: 'Generic Option type',
  },
};

const workspaceSymbols = new Map();
const documentToWorkspace = new Map();

function getSymbolStore(wsKey) {
  const key = wsKey || '__global__';
  if (!workspaceSymbols.has(key)) workspaceSymbols.set(key, new Map());
  return workspaceSymbols.get(key);
}

function resolveWorkspace(docUri) {
  return documentToWorkspace.get(docUri) || '__global__';
}

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;

// Token type indices — order must match SEMANTIC_TOKEN_TYPES array
const TT_KEYWORD     = 0;
const TT_TYPE        = 1;
const TT_FUNCTION    = 2;
const TT_VARIABLE    = 3;
const TT_NUMBER      = 4;
const TT_STRING      = 5;
const TT_COMMENT     = 6;
const TT_OPERATOR    = 7;
const TT_PUNCTUATION = 8;
const TT_PARAMETER   = 9;
const TT_PROPERTY    = 10;
const TT_ENUM_MEMBER = 11;

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
  'parameter',   // 9
  'property',    // 10
  'enumMember',  // 11
];

connection.onInitialize((params) => {
  const caps = params.capabilities;

  hasConfigurationCapability = !!(caps.workspace && caps.workspace.configuration);
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
        moreTriggerCharacter: [],
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
  const store = getSymbolStore(wsKey);

  // Clear stale entries from this file
  for (const [name, info] of store.entries()) {
    if (info.uri === docUri) store.delete(name);
  }

  let m;

  // fn Name(params) -> RetType {
  const fnRx = /\bfn\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([a-zA-Z0-9_<>\[\]]+))?\s*\{/gm;
  while ((m = fnRx.exec(text)) !== null) {
    const name = m[1];
    const params = m[2].trim();
    const ret = m[3] ? m[3].trim() : 'void';
    store.set(name, {
      kind: CompletionItemKind.Function,
      detail: `fn ${name}(${params}) -> ${ret}`,
      doc: 'User-defined function',
      uri: docUri,
    });
  }

  // Top-level PascalCase function (old style): Name(params) -> RetType {
  const legacyFnRx = /^([A-Z][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([a-zA-Z0-9_<>\[\]]+))?\s*\{/gm;
  while ((m = legacyFnRx.exec(text)) !== null) {
    const name = m[1];
    const params = m[2].trim();
    const ret = m[3] ? m[3].trim() : 'void';
    if (!store.has(name)) {
      store.set(name, {
        kind: CompletionItemKind.Function,
        detail: `${name}(${params}) -> ${ret}`,
        doc: 'User-defined function',
        uri: docUri,
      });
    }
  }

  // class Name
  const classRx = /\bclass\s+([A-Z][a-zA-Z0-9_]*)/g;
  while ((m = classRx.exec(text)) !== null) {
    store.set(m[1], { kind: CompletionItemKind.Class, detail: `class ${m[1]}`, doc: 'User-defined class', uri: docUri });
  }

  // struct Name
  const structRx = /\bstruct\s+([A-Z][a-zA-Z0-9_]*)/g;
  while ((m = structRx.exec(text)) !== null) {
    store.set(m[1], { kind: CompletionItemKind.Struct, detail: `struct ${m[1]}`, doc: 'User-defined struct', uri: docUri });
  }

  // enum / sum Name
  const enumRx = /\b(?:enum|sum)\s+([A-Z][a-zA-Z0-9_]*)/g;
  while ((m = enumRx.exec(text)) !== null) {
    store.set(m[1], { kind: CompletionItemKind.Enum, detail: `enum ${m[1]}`, doc: 'Enum / sum type', uri: docUri });
  }

  // Local / typed variables: i32 name, bool name, let name, etc.
  const typePattern = TYPES.join('|');
  const varRx = new RegExp(`\\b(?:${typePattern})(?:<[^>]+>)?(?:\\[\\])*\\s+([a-z_][a-zA-Z0-9_]*)`, 'g');
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
  } catch (_) { }
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
  connection.sendDiagnostics({ uri: event.document.uri, diagnostics: [] });
});

connection.onDocumentOnTypeFormatting((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;

  const text = doc.getText();
  const offset = doc.offsetAt(params.position);

  if (offset < 2) return null;
  const before2 = text.slice(offset - 2, offset);
  if (before2 !== '/!') return null;

  const textBefore = text.slice(0, offset - 2);
  const opens = (textBefore.match(/\/!/g) || []).length;
  const closes = (textBefore.match(/!\//g) || []).length;
  if (opens > closes) return null;

  return [{
    range: { start: params.position, end: params.position },
    newText: '  !/',
  }];
});

connection.onHover((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;

  const text = doc.getText();
  const offset = doc.offsetAt(params.position);

  let start = offset;
  let end = offset;
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
  const query = params.query.toLowerCase();
  const results = [];

  for (const [, store] of workspaceSymbols) {
    for (const [name, info] of store) {
      if (!query || name.toLowerCase().includes(query)) {
        results.push({
          name,
          kind: info.kind,
          location: {
            uri: info.uri,
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

  KEYWORDS.forEach(kw => completions.push({
    label: kw,
    kind: CompletionItemKind.Keyword,
    data: { type: 'keyword', name: kw },
  }));

  TYPES.forEach(t => completions.push({
    label: t,
    kind: CompletionItemKind.TypeParameter,
    data: { type: 'builtin_type', name: t },
  }));

  Object.keys(BUILTIN_FUNCTIONS).forEach(fn => completions.push({
    label: fn,
    kind: CompletionItemKind.Function,
    detail: BUILTIN_FUNCTIONS[fn].sig,
    data: { type: 'builtin_fn', name: fn },
  }));

  Object.entries(SNIPPETS).forEach(([label, snip]) => completions.push({
    label,
    kind: CompletionItemKind.Snippet,
    insertText: snip.insert,
    insertTextFormat: InsertTextFormat.Snippet,
    detail: snip.doc,
    data: { type: 'snippet', name: label },
  }));

  if (doc) {
    const store = getSymbolStore(resolveWorkspace(doc.uri));
    for (const [name, info] of store) {
      if (BUILTIN_FUNCTIONS[name] || KEYWORDS.includes(name) || TYPES.includes(name)) continue;
      completions.push({
        label: name,
        kind: info.kind,
        detail: info.detail,
        data: { type: 'user_symbol', name },
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
  const text = textDocument.getText();
  const diagnostics = [];

  // Brace balance check
  const openBraces = (text.match(/\{/g) || []).length;
  const closeBraces = (text.match(/\}/g) || []).length;
  if (openBraces !== closeBraces) {
    diagnostics.push({
      severity: 1,
      range: { start: textDocument.positionAt(0), end: textDocument.positionAt(text.length) },
      message: `Unmatched braces: ${openBraces} opening vs ${closeBraces} closing`,
      source: 'nexus-lsp',
    });
  }

  // /! block comment balance check
  const openComments = (text.match(/\/!/g) || []).length;
  const closeComments = (text.match(/!\//g) || []).length;
  if (openComments !== closeComments) {
    diagnostics.push({
      severity: 2,
      range: { start: textDocument.positionAt(0), end: textDocument.positionAt(text.length) },
      message: `Unclosed block comment: ${openComments} /!  but ${closeComments} !/`,
      source: 'nexus-lsp',
    });
  }

  connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

// ─── Semantic tokens ──────────────────────────────────────────────────────────

connection.languages.semanticTokens.on((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return { data: [] };

  const text = doc.getText();
  const data = [];
  let pos = 0;
  let line = 0, col = 0;
  let lastLine = 0, lastCol = 0;

  const pushToken = (tLine, tCol, length, tType) => {
    const lineDelta = tLine - lastLine;
    const colDelta  = lineDelta === 0 ? tCol - lastCol : tCol;
    data.push(lineDelta, colDelta, length, tType, 0);
    lastLine = tLine;
    lastCol  = tCol;
  };

  const advanceTo = (target) => {
    for (let i = pos; i < target; i++) {
      if (text[i] === '\n') { line++; col = 0; } else { col++; }
    }
  };

  while (pos < text.length) {
    const ch = text[pos];

    // Newline
    if (ch === '\n') { line++; col = 0; pos++; continue; }

    // Whitespace
    if (/[ \t\r]/.test(ch)) { col++; pos++; continue; }

    // ── // single-line comment ──────────────────────────────────────────────
    if (text.startsWith('//', pos)) {
      const sl = line, sc = col;
      let end = text.indexOf('\n', pos);
      if (end === -1) end = text.length;
      pushToken(sl, sc, end - pos, TT_COMMENT);
      advanceTo(end);
      pos = end;
      continue;
    }

    // ── /* */ single-line block comment ────────────────────────────────────
    if (text.startsWith('/*', pos)) {
      const sl = line, sc = col, start = pos;
      let end = text.indexOf('*/', pos + 2);
      if (end === -1) end = text.length; else end += 2;
      // Emit one token per line for multi-line /* */ comments
      const seg = text.slice(start, end).split('\n');
      for (let i = 0; i < seg.length; i++) {
        if (seg[i].length > 0) pushToken(sl + i, i === 0 ? sc : 0, seg[i].length, TT_COMMENT);
      }
      advanceTo(end);
      line = sl + seg.length - 1;
      col = seg[seg.length - 1].length;
      pos = end;
      continue;
    }

    // ── /! !/ block comment ────────────────────────────────────────────────
    if (text.startsWith('/!', pos)) {
      const sl = line, sc = col, start = pos;
      let end = text.indexOf('!/', pos + 2);
      if (end === -1) end = text.length; else end += 2;
      const seg = text.slice(start, end).split('\n');
      for (let i = 0; i < seg.length; i++) {
        if (seg[i].length > 0) pushToken(sl + i, i === 0 ? sc : 0, seg[i].length, TT_COMMENT);
      }
      advanceTo(end);
      line = sl + seg.length - 1;
      col = seg[seg.length - 1].length;
      pos = end;
      continue;
    }

    // ── char literal 'x' or '\n' ───────────────────────────────────────────
    if (ch === "'") {
      const sl = line, sc = col;
      let end = pos + 1;
      if (end < text.length && text[end] === '\\') end += 2; else end++;
      if (end < text.length && text[end] === "'") end++;
      pushToken(sl, sc, end - pos, TT_STRING);
      col += end - pos;
      pos = end;
      continue;
    }

    // ── string literal "..." ───────────────────────────────────────────────
    if (ch === '"') {
      const sl = line, sc = col, start = pos;
      let end = pos + 1;
      while (end < text.length && !(text[end] === '"' && text[end - 1] !== '\\')) end++;
      if (end < text.length) end++;
      pushToken(sl, sc, end - start, TT_STRING);
      advanceTo(end);
      pos = end;
      continue;
    }

    // ── number literal ─────────────────────────────────────────────────────
    if (/[0-9]/.test(ch) || (ch === '.' && /[0-9]/.test(text[pos + 1] || ''))) {
      const sl = line, sc = col, start = pos;
      // hex  0x...
      if (ch === '0' && (text[pos + 1] === 'x' || text[pos + 1] === 'X')) {
        pos += 2;
        while (pos < text.length && /[0-9a-fA-F_]/.test(text[pos])) pos++;
      } else {
        while (pos < text.length && /[0-9_]/.test(text[pos])) pos++;
        if (pos < text.length && text[pos] === '.') {
          pos++;
          while (pos < text.length && /[0-9_]/.test(text[pos])) pos++;
        }
        if (pos < text.length && /[eE]/.test(text[pos])) {
          pos++;
          if (pos < text.length && /[+\-]/.test(text[pos])) pos++;
          while (pos < text.length && /[0-9_]/.test(text[pos])) pos++;
        }
      }
      pushToken(sl, sc, pos - start, TT_NUMBER);
      col += pos - start;
      continue;
    }

    // ── identifier / keyword / type / builtin ──────────────────────────────
    if (/[a-zA-Z_]/.test(ch)) {
      const sl = line, sc = col, start = pos;
      while (pos < text.length && /[a-zA-Z0-9_]/.test(text[pos])) pos++;
      const word = text.slice(start, pos);

      // Peek ahead past whitespace to see if a '(' follows (= call site)
      let peek = pos;
      while (peek < text.length && /[ \t]/.test(text[peek])) peek++;
      const nextCh = text[peek];

      let tt;
      if (KEYWORDS.includes(word)) {
        tt = TT_KEYWORD;
      } else if (BUILTIN_FUNCTIONS[word]) {
        tt = TT_FUNCTION;
      } else if (nextCh === '(') {
        // anything immediately before '(' is a call — PascalCase or not
        tt = TT_FUNCTION;
      } else if (TYPES.includes(word)) {
        tt = TT_TYPE;
      } else if (/^[A-Z]/.test(word)) {
        // PascalCase with no '(' following — type / class / enum / struct
        tt = TT_TYPE;
      } else {
        tt = TT_VARIABLE;
      }

      pushToken(sl, sc, word.length, tt);
      col += word.length;
      continue;
    }

    // ── operators ──────────────────────────────────────────────────────────
    if ('+-*/%=<>!&|^~@'.includes(ch)) {
      pushToken(line, col, 1, TT_OPERATOR);
      col++; pos++;
      continue;
    }

    // ── punctuation ────────────────────────────────────────────────────────
    if ('(){}[],.;:'.includes(ch)) {
      pushToken(line, col, 1, TT_PUNCTUATION);
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
