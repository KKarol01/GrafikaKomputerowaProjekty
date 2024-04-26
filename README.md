# Archiwum z projektami na zajęcia projektowe z grafiki komputerowej.

## Projekty do wykonania:
- ![Wirtualna kamera](https://github.com/KKarol01/GrafikaKomputerowaProjekty/tree/zad1)
- ![Rozwiązanie problemu zasłaniania](https://github.com/KKarol01/GrafikaKomputerowaProjekty/tree/zad2)
- ![Modelowanie światła](https://github.com/KKarol01/GrafikaKomputerowaProjekty/tree/zad3)

### We wszystkich projektach sterowanie kamerą jest następujące:
- Przemieszczanie: W, A, S, D
- Obrót w osi Z (roll): Q, E
- Rozglądanie się: Ruch myszą góra-dół, lewo-prawo
- Ruch w dół i górę: LShift i Spacja
- Zoom i oddalenie: Kółko myszy
- W zadaniu drugim wyłączenie algorytmu do eliminacji elementów zasłoniętych można przełączać klawiszem "1" oraz można zwizualizować bufor Z klawiszem "2".

## Zrzuty ekranu
### Zadanie 1 (Quaternion, roll, zoom):
![zad1](https://github.com/KKarol01/GrafikaKomputerowaProjekty/assets/20555497/4b4fd5ec-10a6-4f24-925a-c7c4db9f7d0a)

### Zadanie 2 (Z-Buffer):
![zad2](https://github.com/KKarol01/GrafikaKomputerowaProjekty/assets/20555497/27dcd87d-48ca-4443-94dc-cdde9d511bd4)

### Zadanie 3 (Blinn-Phong, Fresnel(Shlick), Raymarching SDFs):
![zad3](https://github.com/KKarol01/GrafikaKomputerowaProjekty/assets/20555497/c3bbabfd-397d-4790-a493-9cd5b6fd31b5)

## Wykorzystane biblioteki:
- Imgui - interfejs
- Raylib - tylko dla zadanie 1, API, interfejs
- GLM - operacje matematyczne
- Glad - obsługa inicjalizacji funkcji OpenGL
- GLFW - tworzenie okna programu

## Technologia:
- Zadanie 1 - Raylib
- Zadania 2 i 3 - OpenGL

## Kompilacja:
Należy mieć nowy kompilator C++ i CMake.
Polecenie do kompilacji: `cmake -S . -B build [-GNinja] && cmake --build build/ --config=Release`
