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

## Ellenőrzés (2026-10-19)

- Build: https://github.com/edeori/WhammyDT-VST/actions/runs/39263444328 — mindkét platform sikeres, Mac aláírás/notarizáció rendben, Windows Defender találat nélkül.
- Külön Publish-próba: https://github.com/edeori/WhammyDT-VST/actions/runs/39263482405 — artifactok és Defender-hash ellenőrzése sikeres, `verify_only=true`, feltöltés kihagyva.
- A teszt a GitHubon lévő 0.4.4-es kódot építette, a még nem commitolt helyi 0.4.5-módosításokat nem tartalmazza. Az éles Mac 0.4.5-ös telepítő változatlan tartalommal került az állandó `WhirlDT.pkg` címre; SHA-256 egyezés ellenőrizve. A próba nem írta felül sem a Mac-, sem a Windows-letöltést. A tesztbuild éles publikálása verzió-visszaállítás lehet, ezért a kívánt forrásverzió commitolása után indíts új buildet.

## Build kiválasztása a Publish indításakor

A `build_selection` legördülő alapértéke **Latest successful build**. Ilyenkor a `build_run_id` mezőt hagyd üresen: a workflow az alapértelmezett (`main`) ág legutóbb létrehozott sikeres buildjét választja. PR-/másik ági buildek nem választhatók automatikusan. Régi futás újrafuttatása nem lesz ettől a legújabb build. A kiválasztás a Publish végrehajtásakor történik, a futásazonosító egyszer rögzül minden artifacthoz.

Konkrét vagy tagről készült buildhez válaszd a **Specific build ID** opciót, majd add meg a `build_run_id` értékét. Mindkét mód használható `verify_only=true` mellett is. A kiválasztott build linkje és forráscommitja megjelenik az összesítőben; hiányzó/lejárt artifact vagy hibás Defender-jelentés esetén a Publish leáll.

## Live versioned release — 2026-10-19

Version **0.4.6**, built source `ecb88aeb4120b7a8bc5275429ff44c449c5cf352`.
[Build](https://github.com/edeori/WhammyDT-VST/actions/runs/39309425298) and [publication](https://github.com/edeori/WhammyDT-VST/actions/runs/39310084254) succeeded for macOS and Windows.
The published Mac package version and Windows ProductVersion were verified; the
live EXE SHA-256 matches the clean Defender report. Shop version updated after
publication. See the [shared release record](https://github.com/edeori/vst-release-pipeline/blob/main/docs/releases/2026-10-19-version-rollout.md) for all products and hashes.
