use zed_extension_api::{
    self as zed, settings::LspSettings, Command, LanguageServerId, Result,
};

struct RunesExtension;

impl zed::Extension for RunesExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        language_server_id: &LanguageServerId,
        worktree: &zed::Worktree,
    ) -> Result<Command> {
        let configured = LspSettings::for_worktree(language_server_id.as_ref(), worktree)
            .ok()
            .and_then(|settings| settings.binary);
        let arguments = configured
            .as_ref()
            .and_then(|binary| binary.arguments.clone())
            .unwrap_or_default();
        let command = configured
            .and_then(|binary| binary.path)
            .or_else(|| worktree.which("runes-lsp"))
            .ok_or_else(|| {
                "runes-lsp was not found; run `make runes-lsp` and add the \
                 repository root to PATH, or configure lsp.runes-lsp.binary.path"
                    .to_owned()
            })?;

        Ok(Command {
            command,
            args: arguments,
            env: Vec::new(),
        })
    }
}

zed::register_extension!(RunesExtension);
