# Security

SKIrf Wardriver is a passive listener. It does not attack, jam or connect to other people's networks.

Two parts of it deserve care:

- **The web portal hotspot** (`SKIrf_WARDRIVER`, http://192.168.4.1) has **no password**. It is up for 60 seconds at each start, or after Settings > Web portal, and while it is up anyone in range can join it and list, download, delete or upload your logs. Tap SKIP when you do not need it, and do not open it in a crowded place. Saved passwords and API keys are never sent back to the browser.
- **Saved credentials** (your home Wi-Fi password, WiGLE API token and WDGWars API key) are stored in a plain file, `wifi_cfg.json`, on the microSD card (or in the board's flash when there is no card). Anyone holding the card can read them. Keep the card safe, and revoke the API keys from your WiGLE and WDGWars accounts if it is lost.

Uploads to WiGLE and WDGWars are encrypted (HTTPS), but in this beta the board does **not check the servers' certificates**, so someone who controls the network you upload through could intercept your API keys. Upload over a network you trust, such as your home Wi-Fi.

If you find a security problem, please use GitHub's **Report a vulnerability** option (Security tab > Advisories) rather than a public issue, so it can be fixed before details are shared.

This is a beta. Do not rely on it for safety-critical decisions.
