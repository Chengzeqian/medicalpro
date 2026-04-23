# Platform Plugin Legacy Consumer Inventory

Updated: 2026-04-23

## Current Consumers

| Consumer | Bucket | Status | Notes | Next Step |
| --- | --- | --- | --- | --- |
| `main.cpp` | `forbidden_product_mainline` | enforced | Product startup must remain descriptor-driven and must not call deleted legacy load-policy helpers. | Protected by source contract tests. |
| `runtime_artifact_layout_test` | `forbidden_product_mainline` | enforced | Default runtime artifact acceptance must validate product artifacts only. | Must not require deleted compatibility artifacts. |

## Deleted Compatibility Shell

| Surface | Status | Notes |
| --- | --- | --- |
| `PluginLoadPolicy` | deleted | Former compatibility carrier is removed from source. |
| `config/plugin_load_policy.json` | deleted | Former compatibility projection is no longer shipped. |
| `config/plugin_load_policy_compatibility.md` | deleted | Former sidecar note is removed with the projection. |
| `CTKManager::loadPluginPolicy()` | deleted | Repository no longer recognizes a legacy load-policy entry point. |
| `CTKManager::installPluginsFromDirectory()` | deleted | Repository no longer recognizes a compatibility directory-scan startup side path. |
| `CTKManager::setPluginLoadOrder()` | deleted | Manual legacy load-order override is removed. |
| `CTKManager::getRecommendedLoadOrder()` | deleted | Manual legacy load-order recommendation is removed. |

## Retained Runtime Ownership

- `CTKManager::applyPolicyForPlugin()` remains an execution helper, but runtime bucket and criticality classification continue to resolve through `PlatformCtkPolicyBridge` with explicit `setDescriptorPolicyContext(...)` handoff.
- Product startup truth remains `platform_runtime.json + plugins/descriptors/*.json + PlatformDescriptorLoader`.

## Forbidden New Usage

- `plugin_load_policy` compatibility shell reintroduction is `forbidden_reintroduction`.
- No new product-mainline code may introduce a second plugin-loading truth source.
- Any future legacy loading surface must be treated as new technical debt, not as restoration of approved compatibility residue.

