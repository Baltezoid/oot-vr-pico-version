# The game downloads mods from the page of their author

The project does not give mods. Players want to install mods without a PC. The
Mod list gives only the address of each file. The game downloads the file from
the page of its author.

- The Mod list contains only mods that are public on the page of their author.
- The person who proposes a mod tells its author. The entry links to this
  notice.
- The maintainers examine the SHA-256 and the install. They do not examine the
  rights to the mod.
- The game reads the list from `main`. A removal applies to all headsets.
- When the author or a rights holder asks, the maintainers remove the entry
  without a discussion.

## Considered options

- Host the mods: then the project distributes them.
- Browse GameBanana in the game: then nobody examines the files.
- Put the list in the APK: then a removal does not get to old APKs.
- Ask the author for permission first: the mod is already public, and many
  authors do not answer.
