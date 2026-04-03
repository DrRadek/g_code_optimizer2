# Rozšíření do sliceru Cura

Rozšíření umožňuje využít `g_code_optimizer2` přímo ve sliceru Cura.

## Požadavky
- Slicer Cura 5.10

## Instalace
Zkopírujte `g_code_optimizer2_plugin` do složky plugins nacházející se v instalaci Sliceru Cura.

Případně můžete místo kopírování vytvořit symbolický odkaz.

Příklad pro operační systém Windows:
`mklink /j "C:\Users\<uživatel>\AppData\Roaming\cura\<verze>\plugins\g_code_optimizer2_plugin" ".\g_code_optimizer2_plugin"`

## Konfigurace
Otevřete `g_code_optimizer2_plugin\settings.json` a vyplňte nastavení:
1.  Zadejte cestu k `g_code_optimizeru 2`

## Použití
1. Ve sliceru vyberte modely, které chcete optimalizovat,
1. Vyberte `Extensions -> g_code_optimizer2 -> optimize orientation`,
1. Počkejte na dokončení algoritmu,
1. Model se automaticky natočí.

Pro výběr a nastavení algoritmů využijte konfiguraci `g_code_optimizeru 2`.

### Ladění
Zdrojové soubory používané slicerem Cura se nacházejí ve složce UM v instalaci Cura.

Pro účely ladění kódu lze vytvořit symbolický odkaz.

Příklad pro operační systém Windows:
`mklink /j ".\g_code_optimizer2_plugin\UM" "C:\Program Files\UltiMaker Cura <verze>\UM"`
