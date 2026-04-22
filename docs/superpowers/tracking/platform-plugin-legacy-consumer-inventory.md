# Platform Plugin Legacy Consumer Inventory

Updated: 2026-04-22

## Current Consumers

| Consumer | Bucket | Status | Notes | Next Step |
| --- | --- | --- | --- | --- |
| `main.cpp` | `forbidden_product_mainline` | enforced | Product startup must remain descriptor-driven and must not call legacy load-policy helpers. | Protected by source contract tests. |
| `config/plugin_load_policy.json` | `allowed_compatibility_surface` | retained | Compatibility-only runtime metadata. | Covered by dedicated compatibility runtime contract. |
| `config/plugin_load_policy_compatibility.md` | `allowed_compatibility_surface` | retained | Sidecar note that explains the compatibility-only boundary. | Covered by dedicated compatibility runtime contract. |
| `CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface` | retained | Compatibility entry for legacy policy metadata loading. | Must not be called by product mainline. |
| `CTKManager::installPluginsFromDirectory()` | `allowed_compatibility_surface` | retained | Compatibility directory-scan helper. | Must not be called by product mainline. |
| `runtime_artifact_layout_test` | `forbidden_product_mainline` | enforced | Default runtime artifact acceptance must validate product artifacts only. | Must not require `plugin_load_policy.json`. |
| `plugin_legacy_compatibility_runtime_contract_test` | `allowed_compatibility_surface` | retained | Dedicated runtime acceptance for compatibility-only artifacts. | Keep it separate from product runtime layout acceptance. |

## Retired Internal Debt

- `CTKManager::policyForPlugin()` is retired from runtime classification ownership and is no longer a governed legacy consumer.
- `CTKManager::applyPolicyForPlugin()` remains as an execution helper, but runtime bucket and criticality classification now resolve through `PlatformCtkPolicyBridge` with explicit `setDescriptorPolicyContext(...)` handoff.

## Forbidden New Usage

- No new product-mainline code may call `loadPluginPolicy()` or `installPluginsFromDirectory()`.
- No new product-mainline code may read `plugin_load_policy.json` to decide startup content.
- Any newly discovered legacy consumer must be added to this inventory before it can be considered acceptable.

