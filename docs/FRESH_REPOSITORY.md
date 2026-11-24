# Starting a fresh repository

Use a separate source folder. Keep this working checkout and its Git history
intact. A new repository in a folder without the old `.git` directory starts with
new history; changing an existing remote does not remove history.

Before copying, review the exact source files for publication. Include current
uncommitted edits and newly added source/tool files, and preserve upstream
licenses, notices and credits. Exclude `.git`, `platforms/out`, build caches,
executables/debug files, extracted game assets, logs, saves, personal settings
and editor/session state. Keep `.gitignore`. The current AGENTS.md contains
machine-specific paths, so exclude it or prepare a neutral public version.
The current development EXE also contains personal build paths; repository
cleanup alone does not make a distributable binary privacy-clean.

In the NEW, reviewed source folder, use your chosen public alias and GitHub's
no-reply email copied from your GitHub email settings. Replace the placeholders:

```powershell
git init -b main
git config user.name "YOUR_PUBLIC_ALIAS"
git config user.email "YOUR_GITHUB_NOREPLY_EMAIL"
git add --dry-run .
```

Review that candidate list, then stage and inspect the exact content before
committing. These are instructions for you, not actions already performed:

```powershell
git add .
git diff --cached --stat
git diff --cached
git commit -m "Initial source snapshot"
```

Create an empty GitHub repository, initially private, without adding another
README/license/gitignore. Keep the licenses already present in the source copy.
Copy its HTTPS URL and connect only the NEW local repository:

```powershell
git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_NEW_REPOSITORY.git
git remote -v
git push -u origin main
```

The new repository has only its new commits. It will not have the old commit
emails/history, but information in the copied files or a release binary is still
visible. Review before making it public. No repository was created, staged,
committed or published by this cleanup task.

A source repository and a playable release ZIP are separate artifacts. The
current combat test requires ignored generated assets and owned game data; a
fresh source copy alone does not replace the pending portable-launcher/asset-
preparation work. Use an explicit release-file list rather than uploading the
whole checkout or output directory.

GitHub reference:
https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github?platform=windows
