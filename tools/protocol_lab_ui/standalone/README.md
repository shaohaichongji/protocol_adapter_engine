# Protocol Lab installed-SDK standalone validation

本页既有 clean-checkpoint 验证身份为 `98df5e0`。最新 G1/G2 仓库构建体验另见 [统一交付入口](../../../deliverables/README.md)，不能将其复制归集当作本页 standalone 组合已重新验证。

This entry builds the complete Qt Lab from the copied Lab whitelist, one installed static or shared PAE SDK,
the fixed Qt 5.13 input and the locked Lab-owned yyjson dependency. It never adds the PAE source
tree as a subdirectory and links Lab targets only to `PAE::pae`.

Use `PrepareStandaloneInputs.ps1` from the repository with explicit `-RepositoryRoot`,
`-DestinationRoot`, `-SdkCandidateRoot` and `-PackageKind static|shared`; the destination must not
already exist. The copied CMake source is
`<DestinationRoot>/inputs/lab/tools/protocol_lab_ui/standalone`. Configure Debug and Release in
separate build directories with Visual Studio x64 and set `PAE_SDK_ROOT` to the exact matching
`<DestinationRoot>/inputs/sdk/pae-sdk-<kind>-<configuration>` package. Also set `PAE_QT_ROOT` to
`<DestinationRoot>/inputs/qt`, `PAE_LAB_DEPENDENCY_ROOT` to
`<DestinationRoot>/inputs/yyjson`, and `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED` to match the
selected SDK. Use `-T v142,version=14.29.30133`. Set `PAE_LAB_BUILD_TESTING=OFF` for the
product-only closure check. Shared builds
validate the imported library and `pae.dll` against the exact SDK root and package manifest, deploy
the matching runtime, and place the same validated DLL beside shared test executables without using
the development tree or global PATH.

The current verified SDK root is
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\sdk\98df5e0`.
All five packages identify `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` with
`source_worktree_dirty=false`. That identity applies to the detached packaging source, not to the
shared working tree that may contain documentation changes. This entry is still a local validation
route, not a release packager or distribution statement.

## Current verified local products

For ordinary local use, prefer the static Release product:

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\static-release\pae_protocol_lab_ui.exe'
```

Its configuration directory is
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\static-release\configs`.
The shared comparison product is
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\shared-release\pae_protocol_lab_ui.exe`;
keep its sibling `pae.dll`, Qt DLLs, `platforms` and `configs` in place. These are bounded local
validation deployments and do not replace any earlier deployment.

The matching evidence is [clean-checkpoint SDK validation](../../../docs/engineering/pae-sdk-clean-checkpoint-validation.md)
and [same-batch Lab consumption validation](../../../docs/engineering/lab-clean-sdk-consumption-validation.md).
This README was synchronized after those runs. Product sources, CMake and scripts were not changed,
but the current 99-file whitelist is no longer byte-for-byte identical to the recorded validation
snapshot because this documentation file changed; no rebuild or retest was performed for this edit.

Human-oriented reading order: start with [project scope](../../../docs/guides/01-项目定位与能力边界.md),
then use [Windows SDK integration](../../../docs/guides/03-Windows-SDK集成.md) for ordinary consumers or
[build, test, and troubleshooting](../../../docs/guides/06-构建测试与问题定位.md) for this standalone route.
