# The game downloads mods from the page of their author

The project does not give mods. Players want to install mods without a PC. The
Mod list gives only the address of each file. The game downloads the file from
the page of its author.

- The Mod list contains only mods that are public on the page of their author.
- Each entry gives the author and links to the page of the mod.
- The game tells the player that the mod is not part of oot-vr.
- The maintainers examine the SHA-256 and the install. They do not examine the
  rights to the mod.
- The game reads the list from `main`. A removal applies to all headsets.
- When a person asks for a removal in an issue, the maintainers remove the
  entry without a discussion.

## Considered options

- Host the mods: then the project distributes them.
- Browse GameBanana in the game: then nobody examines the files.
- Put the list in the APK: then a removal does not get to old APKs.
- Ask the author for permission first: the mod is already public, and many
  authors do not answer.
- Tell the author before each new entry: many authors cannot be contacted.
  The entry links to the page of the author, and a removal is easy.
