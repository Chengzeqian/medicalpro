# plugin_load_policy compatibility note

- `config/plugin_load_policy.json` is a compatibility-only runtime projection.
- Projection source comes from `config/platform_runtime.json` and `plugins/descriptors/*.json`.
- The projection is limited to the descriptor-governed CTK plugin set: `UserManagement`, `DicomViewer`, `FourViewDisplay`, `RegistrationCore`, and `OpticalTracking`.
- `CTKManager::loadPluginPolicy()` may load this projection for compatibility paths, but it must not define the product mainline or CTK runtime classification semantics.
