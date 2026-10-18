# Making a release

For the maintainer. Do these in order.

1. Set `SKIRF_VERSION` and `SKIRF_APP_RELEASE` at the top of the sketch, and the `NAME` in `tools/make_release_bin.py`. Update `CHANGELOG.md` and `RELEASE_NOTES.md`.
2. Build the sketch (see [BUILDING.md](BUILDING.md)), then run `python3 tools/make_release_bin.py src/SKIrf_WARDRIVER_C5_CYD/build/espressif.esp32.esp32c5 out`. Keep the files it writes.
3. Flash the image on a real board. Confirm the boot screen shows the release under the logo and the colours are right. Walk the basics: calibration, the web portal, a short run with a GPS fix (counters move, a CSV appears in Logs), STOP and the Run report; check the Serial Monitor (115200) for errors.
4. Rebuild the user guide PDFs if the screens or steps changed.
5. Commit and push. On GitHub: **Releases > Draft a new release**.
   - Tag: `v0.1.0-beta.1` (create it on publish), target `main`
   - Title: `SKIrf Wardriver for NM-CYD-C5 - Beta 1`
   - Description: paste `RELEASE_NOTES.md`
   - Attach `SKIrf_WARDRIVER_C5_CYD_Beta_1.bin`, `SHA256SUMS.txt`, and the three `SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide*.pdf` files
   - Tick **Set as a pre-release**
6. Settings > General: add the description and topics, and upload `docs/images/social-preview.png` as the social preview.
7. Open the release page in a private window, download the `.bin` once and check it against `SHA256SUMS.txt`.
