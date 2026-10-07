# Решения реализации

- Ruling: Create a sibling isolated worktree as part of approved implementation — avoids editing main — no product behavior cost.

- Ruling: Preserve conflicting upstream README/gitignore/CI under docs/upstream, retain project README and own CI — avoids overwriting project instructions or activating inherited workflows — original files remain available with provenance.

- Ruling: Store payload uses length-prefixed canonical scenario JSON records rather than one large JSON array — bounded per-rule parsing avoids a second entire cJSON tree on C6 — internal storage schema is not an API contract.

- Ruling: Shared codec conditions are flat preorder node arrays with child indices, matching bounded C++ tree; UI will render nesting — avoids separate recursive data model — API clients must use documented schema.

- Ruling: Use pinned Playwright1.56.0 Chromium141 for browser verification — latest1.63 artifact download truncated — tests use standard Playwright APIs, browser version differs from user browser.

- Ruling: Extract simulator API into shared HubRuntime rather than reimplement target handlers — prevents host/target contract drift — target executes the same codec/API through the service request queue.

- Ruling: Portable DeviceModel owns channel telemetry; legacy Core keeps endpoint1 primary bool; explicit endpoint/native-channel fields traverse core command/event/effect/HAL — avoids duplicate channel state — old API exposes only the primary channel.

- Ruling: Initial target actuator support is observed standard OnOff endpoints only; Tuya commands and translated DP sensor values are not exposed to new rules without exact verified fingerprint maps — no guessed DP mapping — ordered Tuya switch remains hardware-gated.

- Ruling: Slot/configuration copies use bounded heap storage and service task stack24KiB — avoids33KiB stack copies on C6 — actual runtime heap/stack high-water still needs hardware verification.

- Ruling: SDK environment pins component-manager2.4.0 within IDF allowed constraint — manager2.5.2 failed in namespace PID discovery — no IDF source changes; CI standard container may not need the pin.

- Ruling: Product profile compiles core/service tables for16devices and omits the disabled MQTT global object — initial size report left too little RAM for the67KiB shared hub — legacy upstream host profile remains64devices; real heap/stack reserve needs hardware measurement.

- Ruling: Target API responses allocate their actual encoded size rather than a fixed64KiB buffer — avoids unnecessary allocation for each small request — large rule-list responses still require hardware heap validation.

- Ruling: No hardcoded initial AP password; provisioning uses a temporary open local AP, then STA-only — avoids committing credentials and keeps first setup accessible — initial setup must occur in a trusted environment.

- Ruling: Journal returns the complete bounded200-entry ring rather than pagination — small prototype UI always requests the current ring — API pagination remains deferred.

- Ruling: Publish to a feature branch and open a PR, keeping main as the reviewed baseline — implements the user's repository instruction without merging unreviewed work — executable version requires checking out the feature branch until merge.
