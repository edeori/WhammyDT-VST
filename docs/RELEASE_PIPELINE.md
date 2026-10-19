# WhirlDT / WhammyDT release pipeline

A GitHub Actions felületén két külön workflow van:

- **Build installers**: kézzel vagy `v*` tag push után Mac és Windows telepítőt készít. A Mac csomag aláírt/notarizált; a Windows buildet a közös Defender-ellenőrzés vizsgálja. Nem publikál.
- **Publish installers**: kézzel indítható a `main` ágról. A `build_run_id` a sikeres build URL-jének végén lévő szám. Ugyanazokat az artifactokat tölti fel, újrafordítás nélkül. `verify_only=true` esetén csak ellenőriz, nem tölt fel.

A kézi Publish-indítás a jóváhagyás; külön environment gate nincs. A közös workflow ellenőrzi a futás eredetét, sikerességét és a Windows EXE egyezését a tiszta Defender-jelentéssel. Az artifactok 30 napig érhetők el. Régebbi build publikálása visszaállítja a régebbi verziót. A két feltöltés nem atomi; részleges hiba esetén ugyanazt a buildet publikáld újra.

Állandó célok:
- https://download.mothproduction.hu/downloads/plugins/whirldt/mac/WhirlDT.pkg
- https://download.mothproduction.hu/downloads/plugins/whirldt/win/WhirlDT.exe

A termékazonosító `whirldt`, a repo `edeori/WhammyDT-VST`, a plugin neve `WhirlDT`. A projektben jelenleg nincs aktiválási/licencelési modul. A Windows-telepítő nem Authenticode-aláírt; a Defender-vizsgálat nem ad SmartScreen-reputációt.

A Windows-telepítő x64 VST3-at és választható standalone alkalmazást tartalmaz. A Mac csomag AU-t is tartalmaz. A közös workflow rögzített commitról érkezik az `edeori/vst-release-pipeline` repóból, a futások és a titkok a WhammyDT repóban vannak.
