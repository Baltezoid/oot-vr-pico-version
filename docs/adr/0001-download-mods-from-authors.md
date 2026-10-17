# The game downloads mods from the page of their author

Players want to install mods without a PC. The Mod list gives the author, the
page, and the file address of each mod. The game downloads each file from the
page of its author. This project does not keep copies.

- The Mod list contains only mods that are public on the page of their author.
- The maintainers examine the SHA-256 and the install. They do not examine the
  rights to the mod.
- The game reads the list from `main`. A removal applies to all headsets.
- Any person can request a removal in an issue. The maintainers remove the mod.

## Considered options

- Host the mods: then this project distributes them.
- Browse GameBanana in the game: then nobody examines the files.
- Put the list in the APK: then a removal does not get to old APKs.
- Ask the author first: many authors do not answer.
