# Basecamp theming (experimental)



https://github.com/user-attachments/assets/2dcf22f0-e728-4583-9942-d6a6f9d2af27



Logos Basecamp 0.3.2 with a data-only theme engine: themes are JSON files (colours, corner
rounding, font family), applied live, picked or uploaded in **Settings → Appearance**.

> **v2 experiment** (gradients, backgrounds, icon packs, per-control styles, density): see the
> [`v2` branch](https://github.com/xAlisher/basecamp-theming/tree/v2).
>
> **Basecamp OS** (desktop layout: dock, windows, glass, tiling, Spotlight): see the
> [`basecamp-os-theme` branch](https://github.com/xAlisher/basecamp-theming/tree/basecamp-os-theme).

- `design-system/`: logos-design-system at `88330d0` (the revision Basecamp 0.3.2 pins) + theming commits
- `basecamp/`: logos-basecamp `0.3.2` + theming commits

## Build

Requires Nix with flakes enabled.

```sh
./build.sh
./result/bin/LogosBasecamp
```

## Try a theme

Ready-to-upload theme files are in `themes/` (Paper Serif, Hacker, Catppuccin Mocha). Upload one in
**Settings → Appearance**.

## Docs

- [Theme file spec v1](design-system/docs/theming/README.md): what this build implements
- [Theme v2 proposal](docs/THEME-V2-SPEC.md): gradients, backgrounds, icon packs
- [Design system mobile-readiness audit](docs/DS-MOBILE-AUDIT.md): touch targets, hover, adaptive layout
