#!/usr/bin/env node

const {
  createConnection,
  TextDocuments,
  ProposedFeatures,
  DidChangeConfigurationNotification,
  CompletionItemKind,
  TextDocumentSyncKind,
} = require('vscode-languageserver/node');

const { TextDocument } = require('vscode-languageserver-textdocument');

const connection = createConnection(ProposedFeatures.all);

const documents = new TextDocuments(TextDocument);

const SEMANTIC_TOKEN_TYPES = [
  'keyword',
  'type',
  'function',
  'variable',
  'number',
  'string',
  'comment',
  'operator',
  'punctuation',
];

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

const KEYWORDS = [
  'sum', 'class', 'implement', 'global', 'Constructor', 'Factory',
  'if', 'else', 'for', 'while', 'return', 'match', 'new',
  'self', 'Sequential', 'true', 'false', "public", "private", "protected"
];

const TYPES = [
  'i32', 'i64', 'i16', 'i8', 'i4',
  'u32', 'u64', 'u16', 'u8',
  'f32', 'f64', 'bool', 'string', 'void', 'char'
];

const BUILTIN_FUNCTIONS = [
  'Printf', 'Warnf', 'Math.Random', 'Print', 'Warn'
];


// -------------------------
// Initialize Server
// -------------------------
connection.onInitialize((params) => {
  const capabilities = params.capabilities;

  hasConfigurationCapability = !!(
    capabilities.workspace && !!capabilities.workspace.configuration
  );
  hasWorkspaceFolderCapability = !!(
    capabilities.workspace && !!capabilities.workspace.workspaceFolders
  );
  hasDiagnosticRelatedInformationCapability = !!(
    capabilities.textDocument &&
    capabilities.textDocument.publishDiagnostics &&
    capabilities.textDocument.publishDiagnostics.relatedInformation
  );

  const result = {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      completionProvider: {
        resolveProvider: true,
        triggerCharacters: ['.', ':', '>', '<']
      },
      semanticTokensProvider: {
        legend: {
          tokenTypes: SEMANTIC_TOKEN_TYPES,
          tokenModifiers: []
        },
        full: true
      }
    }
  };

  if (hasWorkspaceFolderCapability) {
    result.capabilities.workspace = {
      workspaceFolders: { supported: true }
    };
  }

  return result;
});

connection.onInitialized(() => {
  if (hasConfigurationCapability) {
    connection.client.register(DidChangeConfigurationNotification.type, undefined);
  }
  if (hasWorkspaceFolderCapability) {
    connection.workspace.onDidChangeWorkspaceFolders(_event => {
      connection.console.log('Workspace folder change event received.');
    });
  }
});

// -------------------------
// Completion Provider
// -------------------------
connection.onCompletion((_textDocumentPosition) => {
  const completions = [];

  KEYWORDS.forEach(keyword => {
    completions.push({ label: keyword, kind: CompletionItemKind.Keyword, data: keyword });
  });

  TYPES.forEach(type => {
    completions.push({ label: type, kind: CompletionItemKind.TypeParameter, data: type });
  });

  BUILTIN_FUNCTIONS.forEach(func => {
    completions.push({ label: func, kind: CompletionItemKind.Function, data: func });
  });

  // Snippets
  completions.push({
    label: 'Options<T>',
    kind: CompletionItemKind.Snippet,
    insertText: 'Options<${1:T}>',
    insertTextFormat: 2,
    data: 'options_template'
  });

  completions.push({
    label: 'class',
    kind: CompletionItemKind.Snippet,
    insertText: 'class ${1:ClassName}\n{\n\t${2}\n}',
    insertTextFormat: 2,
    data: 'class_snippet'
  });

  completions.push({
    label: 'sum',
    kind: CompletionItemKind.Snippet,
    insertText: 'sum ${1:TypeName}\n{\n\t${2:Variant}\n}',
    insertTextFormat: 2,
    data: 'sum_snippet'
  });

  completions.push({
    label: 'match',
    kind: CompletionItemKind.Snippet,
    insertText: 'match (${1:value})\n{\n\t${2:Pattern} => ${3:expression}\n}',
    insertTextFormat: 2,
    data: 'match_snippet'
  });

  return completions;
});

connection.onCompletionResolve((item) => {
  if (KEYWORDS.includes(item.data)) {
    item.detail = 'Nexus keyword';
    item.documentation = `Keyword: ${item.data}`;
  } else if (TYPES.includes(item.data)) {
    item.detail = 'Nexus type';
    item.documentation = `Built-in type: ${item.data}`;
  } else if (BUILTIN_FUNCTIONS.includes(item.data)) {
    item.detail = 'Built-in function';
    item.documentation = `Function: ${item.data}`;
  }
  return item;
});

// -------------------------
// Diagnostics
// -------------------------
documents.onDidChangeContent(change => validateTextDocument(change.document));

async function validateTextDocument(textDocument) {
  const text = textDocument.getText();
  const diagnostics = [];

  const openBraces = (text.match(/\{/g) || []).length;
  const closeBraces = (text.match(/\}/g) || []).length;

  if (openBraces !== closeBraces) {
    diagnostics.push({
      severity: 1,
      range: {
        start: textDocument.positionAt(0),
        end: textDocument.positionAt(text.length)
      },
      message: `Unmatched braces: ${openBraces} opening, ${closeBraces} closing`,
      source: 'nexus-lsp'
    });
  }

  connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

// -------------------------
// Semantic Tokens
// -------------------------
connection.languages.semanticTokens.on((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return { data: [] };

  const text = doc.getText();
  const data = [];

  let pos = 0;
  let line = 0;
  let col = 0;
  let lastLine = 0;
  let lastCol = 0;

  const getEndPosition = (startPos, endPos) => {
    let currentLine = line;
    let currentCol = col;
    for (let i = startPos; i < endPos; i++) {
      if (text[i] === '\n') {
        currentLine++;
        currentCol = 0;
      } else {
        currentCol++;
      }
    }
    return { line: currentLine, col: currentCol };
  };

  const pushToken = (tokenLine, tokenCol, length, tokenType) => {
    const lineDelta = tokenLine - lastLine;
    const colDelta = lineDelta === 0 ? tokenCol - lastCol : tokenCol;
    data.push(lineDelta, colDelta, length, tokenType, 0);
    lastLine = tokenLine;
    lastCol = tokenCol;
  };

  while (pos < text.length) {
    const ch = text[pos];

    if (ch === '\n') {
      line++;
      col = 0;
      pos++;
      continue;
    }

    if (/\s/.test(ch)) {
      col++;
      pos++;
      continue;
    }

    if (text.startsWith('/*', pos)) {
      const start = pos;
      const startLine = line;
      const startCol = col;
      
      let end = text.indexOf('\n', pos);
      if (end === -1) end = text.length;
      
      const length = end - start;
      pushToken(startLine, startCol, length, 6);
      
      pos = end;
      col += length;
      continue;
    }

    if (text.startsWith('/!', pos)) {
      const start = pos;
      const startLine = line;
      const startCol = col;
      
      let end = text.indexOf('!/', pos + 2);
      if (end === -1) {
        end = text.length;
      } else {
        end += 2;
      }
      
      const length = end - start;
      pushToken(startLine, startCol, length, 6);
      
      const endPos = getEndPosition(start, end);
      line = endPos.line;
      col = endPos.col;
      pos = end;
      continue;
    }

    if (ch === '"') {
      const start = pos;
      const startLine = line;
      const startCol = col;
      
      let end = pos + 1;
      while (end < text.length && (text[end] !== '"' || text[end - 1] === '\\')) {
        end++;
      }
      if (end < text.length) end++;
      
      const length = end - start;
      pushToken(startLine, startCol, length, 5);
      
      const endPos = getEndPosition(start, end);
      line = endPos.line;
      col = endPos.col;
      pos = end;
      continue;
    }

    if (/[0-9]/.test(ch)) {
      const start = pos;
      const startLine = line;
      const startCol = col;
      
      while (pos < text.length && /[0-9.eE+-]/.test(text[pos])) {
        pos++;
      }
      
      const length = pos - start;
      pushToken(startLine, startCol, length, 4);
      col += length;
      continue;
    }

    if (/[a-zA-Z_]/.test(ch)) {
      const start = pos;
      const startLine = line;
      const startCol = col;
      
      while (pos < text.length && /[a-zA-Z0-9_.]/.test(text[pos])) {
        pos++;
      }
      
      const word = text.slice(start, pos);
      const length = word.length;
      
      let tokenType;
      if (KEYWORDS.includes(word)) {
        tokenType = 0;
      } else if (TYPES.includes(word) || /^[A-Z]/.test(word)) {
        tokenType = 1;
      } else if (BUILTIN_FUNCTIONS.includes(word)) {
        tokenType = 2;
      } else {
        tokenType = 3;
      }
      
      pushToken(startLine, startCol, length, tokenType);
      col += length;
      continue;
    }

    if ("+-*/%=<>!&|".includes(ch)) {
      pushToken(line, col, 1, 7);
      col++;
      pos++;
      continue;
    }

    if ("(){}[],.;:".includes(ch)) {
      pushToken(line, col, 1, 8);
      col++;
      pos++;
      continue;
    }

    col++;
    pos++;
  }

  return { data };
});

// -------------------------
// Listen
// -------------------------
documents.listen(connection);
connection.listen();
connection.console.log('Nexus LSP Server started');
