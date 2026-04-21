# plugin_load_policy compatibility note

- `config/plugin_load_policy.json` is a compatibility-only runtime artifact.
- Product startup truth comes from `config/platform_runtime.json` and `plugins/descriptors/*.json`.
- `CTKManager::loadPluginPolicy()` and `CTKManager::installPluginsFromDirectory()` are retained only for legacy compatibility paths and must not define the product mainline.
