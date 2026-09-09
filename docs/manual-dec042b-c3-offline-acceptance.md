# DEC-042B C3 人工离线验收单

状态：`NOT_EVALUATED（未评估）`。本清单只使用公开合成配置和本地文件，不执行网络收发，也不
替代Golden、硬件、现场、Linux、Oracle或性能验证。

2026-09-08代理执行补记：Windows CLI路径编码边界修复后，代理已使用中文及空格绝对路径完成
本清单七项，35项精确检查全部通过；修复前失败证据仍保留。该结果不是用户人工操作，用户人工
状态继续为NOT_EVALUATED，见[代理验收报告](agent-dec042b-c3-offline-acceptance.md)。

2026-09-08用户确认：代理离线验收PASS足以收口本功能检查点；本人工清单保留为后续可用性检查，
不再作为该检查点的阻塞门禁，不将未执行的人工操作追认为通过。

## 1. 准备

在仓库根目录的PowerShell中执行：

```powershell
$lab = Resolve-Path 'out/dec042b-c3-release-msvc/tools/protocol_lab/pae_protocol_lab.exe'
$config = Resolve-Path 'tests/protocol_core/fixtures/decimal_core_contract.pae.json'
$fixtures = Resolve-Path 'tests/protocol_lab_c3_cli/fixtures'
$root = Join-Path (Resolve-Path 'out') ("manual-dec042b-c3-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $root | Out-Null
$record = [ordered]@{}
```

每次调用都先保存`$LASTEXITCODE`，再解析JSON，避免后续命令覆盖退出码。

## 2. 七项操作

### 2.1 动态Decimal Encode

```powershell
$raw = & $lab encode --config $config --values "$fixtures/valid.values.pae-lab.json" --record-root $root --output json
$code = $LASTEXITCODE; $encode = $raw | ConvertFrom-Json
$record.encode = [ordered]@{ exit=$code; terminal=$encode.current_terminal_status; bundle=$encode.published_bundle; frame=$encode.result.frame_hex }
$encode.result.fields | Where-Object kind -EQ 'DECIMAL64' | Select-Object id,kind, `
  @{Name='coefficient';Expression={$_.decimal64.coefficient}}, `
  @{Name='scale';Expression={$_.decimal64.scale}},raw_kind,raw_value | Format-Table
$record.encode
```

预期：退出`0`、终态`OK`、Result为`pae.lab.result/0.6`；Decimal保留规范raw整数和
coefficient/scale，Frame为
`000000000000020BFFFFFFFFFFFFFFFF00000000000000008000000000000000A52A`。

### 2.2 Inspect Encode产物

```powershell
$frame = Join-Path $encode.published_bundle 'frames/000001_frame.bin'
$raw = & $lab inspect --config $config --frame-bin $frame --record-root $root --output json
$code = $LASTEXITCODE; $inspect = $raw | ConvertFrom-Json
$record.inspect = [ordered]@{ exit=$code; terminal=$inspect.current_terminal_status; bundle=$inspect.published_bundle; coefficient=$inspect.result.fields[0].decimal64.coefficient; scale=$inspect.result.fields[0].decimal64.scale }
$record.inspect
```

预期：退出`0`、终态`OK`，首字段`coefficient=123`、`scale=1`，数学值与Encode输入一致。

### 2.3 Replay再Replay

```powershell
$raw = & $lab replay --bundle $inspect.published_bundle --record-root $root --output json
$codeA = $LASTEXITCODE; $replayA = $raw | ConvertFrom-Json
$raw = & $lab replay --bundle $replayA.published_bundle --record-root $root --output json
$codeB = $LASTEXITCODE; $replayB = $raw | ConvertFrom-Json
$record.replay = [ordered]@{ exitA=$codeA; compareA=$replayA.comparison.status; parentA=$inspect.published_bundle; childA=$replayA.published_bundle; exitB=$codeB; compareB=$replayB.comparison.status; childB=$replayB.published_bundle }
$record.replay
```

预期：两次退出均为`0`、比较均为`EQUAL`，三个Bundle路径互不相同且父子链连续。

### 2.4 等价Decimal原文Compare

```powershell
$raw = & $lab encode --config $config --values "$fixtures/equivalent.values.pae-lab.json" --record-root $root --output json
$encodeCode = $LASTEXITCODE; $equivalent = $raw | ConvertFrom-Json
$before = @(Get-ChildItem $root -Directory).Count
$raw = & $lab compare --left-run $encode.published_bundle --right-run $equivalent.published_bundle --output json
$compareCode = $LASTEXITCODE; $compareEquivalent = $raw | ConvertFrom-Json
$after = @(Get-ChildItem $root -Directory).Count
$record.equivalent = [ordered]@{ encodeExit=$encodeCode; compareExit=$compareCode; status=$compareEquivalent.comparison.status; bundleCountBefore=$before; bundleCountAfter=$after }
$record.equivalent
```

预期：Encode与Compare均退出`0`，比较为`EQUAL`，Compare前后目录数相同（零发布）。

### 2.5 改业务值后Compare

```powershell
$raw = & $lab encode --config $config --values "$fixtures/different.values.pae-lab.json" --record-root $root --output json
$encodeCode = $LASTEXITCODE; $different = $raw | ConvertFrom-Json
$raw = & $lab compare --left-run $encode.published_bundle --right-run $different.published_bundle --output json
$compareCode = $LASTEXITCODE; $compareDifferent = $raw | ConvertFrom-Json
$record.different = [ordered]@{ encodeExit=$encodeCode; compareExit=$compareCode; status=$compareDifferent.comparison.status; diagnostic=$compareDifferent.diagnostic.id }
$record.different
```

预期：第二次Encode退出`0`；Compare为`DIFFERENT`、退出`6`，诊断
`PAE_LAB_C3_COMPARE_DIFFERENT`。

### 2.6 SUM8失败与期望匹配

```powershell
$raw = & $lab inspect --config $config --frame-hex "$fixtures/bad_sum8.frame.hex" --record-root $root --output json
$defaultCode = $LASTEXITCODE; $sum8Default = $raw | ConvertFrom-Json
$raw = & $lab inspect --config $config --frame-hex "$fixtures/bad_sum8.frame.hex" --record-root $root --expect-status INTEGRITY_FAILED --output json
$expectedCode = $LASTEXITCODE; $sum8Expected = $raw | ConvertFrom-Json
$record.sum8 = [ordered]@{ defaultExit=$defaultCode; defaultResultExit=$sum8Default.result.exit_code; expectedExit=$expectedCode; matched=$sum8Expected.expectation.matched; expectedResultExit=$sum8Expected.result.exit_code; status=$sum8Expected.result.current_execution_status }
$record.sum8
```

预期：默认进程退出`5`；期望匹配后进程退出`0`，但两份Result仍为`INTEGRITY_FAILED`且
`result.exit_code=5`。

### 2.7 损坏Bundle拒绝且不发布

```powershell
$corruptParent = Join-Path $root 'corrupt-copy'
New-Item -ItemType Directory -Path $corruptParent | Out-Null
Copy-Item -Recurse -LiteralPath $inspect.published_bundle -Destination $corruptParent
$corrupt = Join-Path $corruptParent (Split-Path $inspect.published_bundle -Leaf)
Add-Content -LiteralPath (Join-Path $corrupt 'result_summary_v0.6.json') -Value ' '
$before = @(Get-ChildItem $root -Directory | Where-Object Name -Like 'run_c3_*').Count
$raw = & $lab replay --bundle $corrupt --record-root $root --output json
$code = $LASTEXITCODE; $corruptReplay = $raw | ConvertFrom-Json
$after = @(Get-ChildItem $root -Directory | Where-Object Name -Like 'run_c3_*').Count
$record.corrupt = [ordered]@{ exit=$code; diagnostic=$corruptReplay.diagnostic.id; published=$corruptReplay.published_bundle; before=$before; after=$after }
$record.corrupt
```

预期：退出`3`、诊断`PAE_LAB_C3_REPLAY_EVIDENCE_INVALID`、`published_bundle=null`，前后
`run_c3_*`目录数相同。

## 3. 保存记录

```powershell
$record | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $root 'manual-acceptance-record.json')
$record | ConvertTo-Json -Depth 8
```

人工执行前不得把状态改为PASS。任一预期不符时保留`$root`并记录实际退出码、JSON封装和命令；
不要修改Bundle来追求通过。
