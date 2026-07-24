const vscode = require("vscode");
const {
  LanguageClient,
  TransportKind,
} = require("vscode-languageclient/node");

let client;

function activate(context) {
  const command = vscode.workspace
    .getConfiguration("runes")
    .get("languageServer.path", "runes-lsp");
  const serverOptions = {
    run: { command, transport: TransportKind.stdio },
    debug: { command, transport: TransportKind.stdio },
  };
  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "runes" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.runes"),
    },
  };

  client = new LanguageClient(
    "runes-lsp",
    "Runes Language Server",
    serverOptions,
    clientOptions
  );
  context.subscriptions.push(client.start());
}

async function deactivate() {
  if (client) {
    await client.stop();
  }
}

module.exports = { activate, deactivate };
