# Packaging

Put `dsound.dll`, the 64-bit [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), in this folder
as `packaging/dsound.dll`. The release workflow copies it next to the built `.asi` when it assembles the release zip,
and fails if it is missing.
