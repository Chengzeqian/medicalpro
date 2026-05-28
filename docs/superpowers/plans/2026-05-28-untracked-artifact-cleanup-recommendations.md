# Untracked Artifact Cleanup Recommendations

**日期：** 2026-05-28
**适用范围：** `d:\Qtproject\medicalpro`
**目的：** 对当前主工作区中的未跟踪产物和少量未提交改动进行分类，给出“建议忽略 / 建议保留 / 建议单独处理”的清单，避免后续提交再次混入本地产物。

---

## 1. 当前结论

当前工作区剩余内容可以分成 3 类：

1. 建议加入忽略规则的本地产物
2. 建议保留并纳入版本管理的设计 / 计划文档
3. 建议单独判断的已跟踪改动

本文件只给建议，不直接删除文件，也不直接修改 `.gitignore`。

---

## 2. 建议忽略

### 2.1 运行输出文本

这些文件明显属于 smoke / contract / integration 运行输出，不建议进入版本库：

- `ankle_contract_out.txt`
- `contract_out.txt`
- `meshgpu_smoke_stdout.txt`
- `meshgpu_smoke_stderr.txt`
- `nav_appsvc_test_out.txt`
- `nav_appsvc_test_result.txt`
- `nav_contract_test_output.txt`
- `nav_vtk_test_output.txt`
- `point_reg_integration_stderr.txt`
- `point_reg_integration_stdout.txt`
- `point_registration_stderr.txt`
- `point_registration_stdout.txt`

建议忽略规则：

```gitignore
/*_out.txt
/*_output.txt
/*_stdout.txt
/*_stderr.txt
/contract_out.txt
/ankle_contract_out.txt
```

### 2.2 本地数据库和实验结果

这些内容更像本地运行数据或实验导出结果，默认不建议进主仓库：

- `data/medical.db`
- `summaries/innovation_1_summary.csv`
- `summaries/innovation_2_summary.csv`
- `summaries/innovation_3_summary.csv`

说明：

- `data/medical.db` 是本地数据库文件
- `summaries/*.csv` 是实验摘要导出结果，适合作为论文材料或归档附件，但不适合作为主仓库默认提交内容

建议忽略规则：

```gitignore
/data/*.db
/summaries/*.csv
```

### 2.3 本地几何配置

下面这组更像实验或设备本地配置：

- `geometry/geometry10.ini`
- `geometry/geometry40.ini`
- `geometry/geometry60.ini`

建议：

- 如果它们只是本地调试 / 演示配置，加入忽略
- 如果它们是后续算法复现实验的标准配置样例，就不要忽略，而是转为“明确命名的样例配置”再提交

当前判断更偏向“建议忽略”。

建议忽略规则：

```gitignore
/geometry/*.ini
```

---

## 3. 建议保留并纳入版本管理

这批文件不是运行产物，而是明确的设计 / 实施文档，建议保留并在后续单独整理提交。

### 3.1 新增计划文档

- `docs/superpowers/plans/2026-05-06-navigation-page-full-workflow-implementation-plan.md`
- `docs/superpowers/plans/2026-05-06-navigation-page-workspace-shell-implementation-plan.md`
- `docs/superpowers/plans/2026-05-06-probe-calibration-unified-tracking-implementation-plan.md`
- `docs/superpowers/plans/2026-05-07-case-centered-surgical-workflow-architecture-implementation-plan.md`
- `docs/superpowers/plans/2026-05-07-navigation-page-case-workflow-orchestrator-implementation-plan.md`
- `docs/superpowers/plans/2026-05-07-navigation-workspace-orchestrator-v2-implementation-plan.md`
- `docs/superpowers/plans/2026-05-08-navigation-realtime-pose-digital-twin-implementation-plan.md`
- `docs/superpowers/plans/2026-05-08-navigation-workspace-ui-realignment-implementation-plan.md`
- `docs/superpowers/plans/2026-05-11-registration-workspace-single-algorithm-ui-implementation-plan.md`
- `docs/superpowers/plans/2026-05-26-main_workspace_merge_handoff.md`

### 3.2 新增设计文档

- `docs/superpowers/specs/2026-05-06-navigation-page-full-workflow-design.md`
- `docs/superpowers/specs/2026-05-06-navigation-page-workspace-shell-design.md`
- `docs/superpowers/specs/2026-05-06-probe-calibration-unified-tracking-design.md`
- `docs/superpowers/specs/2026-05-07-case-centered-surgical-workflow-architecture-design.md`
- `docs/superpowers/specs/2026-05-07-navigation-page-case-workflow-orchestrator-design.md`
- `docs/superpowers/specs/2026-05-07-navigation-workspace-orchestrator-v2-design.md`
- `docs/superpowers/specs/2026-05-11-registration-workspace-single-algorithm-ui-design.md`

建议：

- 这批文档不应忽略
- 可以考虑后续单独做一个 `docs:` 提交
- 如果你希望仓库更干净，也可以只保留关键节点文档，把重复或已过时文档移到外部归档目录

---

## 4. 建议单独处理

### 4.1 `medicalpro_zh_CN.ts`

当前状态：

- 它不是未跟踪文件
- 它是已跟踪但未提交的修改

建议：

- 单独检查它的实际 diff
- 如果只是 Qt Linguist 自动更新或无关格式变化，建议不要混入当前主线提交
- 如果它是导航 / 配准新文本的必要翻译，再单独补一个 `i18n` 提交

当前更推荐：**单独判断，不自动提交，也不加入忽略**。

### 4.2 `data/instrumentThumbnails/`

当前看到目录存在，但这次没有列出具体文件。

建议：

- 如果目录下是运行时缓存缩略图，加入忽略
- 如果目录下是正式随仓资源，则应改为受控资源目录并明确提交

当前偏向：**先视为本地缓存目录，不参与主提交**。

---

## 5. 推荐忽略规则草案

如果后面要正式清理 `.gitignore`，建议追加下面这一段：

```gitignore
# Local runtime outputs
/*_out.txt
/*_output.txt
/*_stdout.txt
/*_stderr.txt
/contract_out.txt
/ankle_contract_out.txt

# Local runtime data
/data/*.db
/summaries/*.csv

# Local experiment geometry configs
/geometry/*.ini

# Local generated thumbnails
/data/instrumentThumbnails/
```

说明：

- 我暂时没有把这段直接写进 `.gitignore`
- 因为 `geometry/*.ini` 和 `instrumentThumbnails/` 仍然有小概率属于你想保留的实验 / 资源内容
- 最稳妥的做法是你确认后，我再帮你正式追加

---

## 6. 推荐后续动作

建议按这个顺序清理：

1. 先确认 `medicalpro_zh_CN.ts` 是否需要保留为单独提交
2. 再确认 `geometry/*.ini` 和 `data/instrumentThumbnails/` 是样例资源还是本地缓存
3. 如果确认是本地产物，我再帮你把忽略规则写进 `.gitignore`
4. 最后再决定是否把那批新增 `docs/superpowers` 文档单独提交

---

## 7. 最小名单

### 建议忽略

- 运行输出 `.txt`
- `data/medical.db`
- `summaries/*.csv`
- 大概率还包括 `geometry/*.ini`

### 建议保留

- `docs/superpowers/plans/2026-05-06...2026-05-26` 这批新增文档
- `docs/superpowers/specs/2026-05-06...2026-05-11` 这批新增文档

### 建议单独判断

- `medicalpro_zh_CN.ts`
- `data/instrumentThumbnails/`
