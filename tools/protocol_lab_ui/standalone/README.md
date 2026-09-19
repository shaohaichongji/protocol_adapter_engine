# Protocol Lab installed-SDK standalone validation

This entry builds the complete Qt Lab from the copied Lab whitelist, one installed static or shared PAE SDK,
the fixed Qt 5.13 input and the locked Lab-owned yyjson dependency. It never adds the PAE source
tree as a subdirectory and links Lab targets only to `PAE::pae`.

Use `PrepareStandaloneInputs.ps1 -PackageKind static|shared` from the repository to create a new external input root. Configure
Debug and Release in separate build directories with Visual Studio x64 and
`-T v142,version=14.29.30133`, and set `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED` to match the
selected SDK. Set `PAE_LAB_BUILD_TESTING=OFF` for the product-only closure check. Shared builds
validate the imported library and `pae.dll` against the exact SDK root and package manifest, deploy
the matching runtime, and place the same validated DLL beside shared test executables without using
the development tree or global PATH.

The input SDK has dirty provenance and is for local closure validation only. This entry is not a
release packager.
