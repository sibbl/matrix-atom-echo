import { createBridgeRuntime } from "./server.js";

const runtime = createBridgeRuntime();

runtime.server.listen(runtime.config.bridgePort, runtime.config.bridgeHost, () => {
  console.log(
    `[bridge] listening on ${runtime.config.bridgeBaseUrl} (${runtime.config.replyMode} mode)`
  );
});
