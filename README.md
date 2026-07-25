# g_code_optimizer2
Vytvořeno na základě https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR.

Nástroj příkazového řádku pro optimalizace rotace STL modelu pro 3D tisk napsaný v C++ 20.

První verze: https://github.com/DrRadek/g_code_optimizer

Nástroj se snaží nalézt optimální rotaci pomocí minimalizace objemu podpěr. Pro výpočet využívá rasterizaci nebo sledování paprsků.

Výsledný model je pravoruký a otočen osou Z nahoru (stejně jako PrusaSlicer).


## Instalace
K instalaci použijte Cmake.

```bash
git submodule update --init

cmake -B build -S .
cmake --build build -j 8
```

## Rozšíření do sliceru Cura

[README](python_extensions/cura_slicer/README.MD) rozšíření.

## Pomocné Python skripty

[sphere_volume_visualizer.py](python_extensions/visualizers/sphere_volume_visualizer.py) - pomocná vizualizace minimalizovaného objemu.

[g_code_optimizer2_bridge.py](python_extensions/algos/g_code_optimizer2_bridge.py) - rozhraní pro psaní algoritmů v Pythonu.

## Výstupy
Výstupem je optimalizovaný stl soubor pokud je cesta k výstupnímu soubor specifikována. V opačném případě se výstup neukládá.

## Vstupy

Program podporuje následující přepínače:


|Přepínač|Požadováno|Typ|Výchozí hodnota|Popis|
|:-:|:-:|:-:|:-:|:-:|
|inputStl|Ano|string|N/A|vstupní soubor|
|outputStl|Ne|string|N/A|výstupní stl soubor|
|config|Ne|string|`Cesta_k_exe/config`|cesta ke konfiguračnímu souboru|
|algorithm|Ano při headless|string|N/A|Název algoritmu|
|raytraced|Ne|N/A|N/A|Použije se raytracing namístor rasterizace|
|headless|Ne|N/A|N/A|Program se pustí bez UI|
|closeOnDone|Ne|N/A|N/A|Program se ukončí po skončení běhu algoritmu|
|textureResolution|int|Ano|N/A|Větší rozlišení = přesnější výpočet objemu|
|voxelSpacing|float|Ano|N/A|Menší voxelSpacing = přesnější výpočet objemu|

Dostupné algoritmy: test, basic, deterministic, stochastic a python.
Doporučuji používat vždy deterministic. Python lze využít při implementaci vlastního algoritmu v Pythonu. Data se sdílejí přes sdílenou paměť.

Musíte použít přesně jednu volbu z textureResolution a voxelSpacing. VoxelSpacing počítá textureResolution dynamicky. Maximální roslišení je omezené na 4096 x 4096.

Headless ignoruje closeOnDone a ukončuje program vždy.

## Příklady použití

Spuštění programu v režimu UI bez předvoleného algoritmu a uložením do souboru: `g_code_optimizer2.exe --inputStl ".\moje_stl.stl" --outputStl ".\moje_stl_vystup.stl"`

Zapnutí programu v headless režimu s vybraným algoritmem: `g_code_optimizer2.exe --inputStl ".\moje_stl.stl" --algorithm "deterministic --headless"`

Zapnutí programu v UI režimu při použití použití raytracingu s vybraným algoritmem a s automatickým ukončením po skončení programu:  `g_code_optimizer2.exe --inputStl ".\moje_stl.stl" --algorithm "deterministic --raytraced --closeOnDone"`

Zapnutí programu v UI režimu s vybraným algoritmem a fixním rozlišením 300: `g_code_optimizer2.exe --inputStl ".\moje_stl.stl" --algorithm "deterministic" --textureResolution 300 --voxelSpacing 0`

Zapnutí programu v UI režimu s vybraným algoritmem a dynamickým rozlišením (rozestup pixelů 0.1 jednotek): `g_code_optimizer2.exe --inputStl ".\moje_stl.stl" --algorithm "deterministic" --textureResolution 0 --voxelSpacing 0.1`

## Závislosti
- [nlohmann_json](https://github.com/nlohmann/json.git)
- [nvpro_core2](https://github.com/nvpro-samples/nvpro_core2)

### Tranzitivní závislosti
- [ImGui](https://github.com/ocornut/imgui)
- [Glm](https://github.com/g-truc/glm)

### Header-only
- [OpenSTL](https://github.com/Innoptech/OpenSTL)

