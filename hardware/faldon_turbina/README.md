# Faldón de turbina

Pieza de TPU que cierra el espacio entre la placa principal (Rapiduino V1.0) y
la pista alrededor de la turbina 8520 de 55 mm, para que la turbina genere
depresión en vez de solo mover aire. Es una sola pieza: una **bandeja** plana
de 1,6 mm que se atornilla debajo de la placa y **aletas** de 0,6 mm en todo el
perímetro que bajan hasta 0,5 mm de la pista.

Modelo paramétrico: [`faldon_turbina.scad`](faldon_turbina.scad) (OpenSCAD 2021.01 o
posterior). Las coordenadas salen de `hardware/main_pcb/Rapiduino.kicad_pcb`.

## Criterios de diseño

- **Que no cargue peso.** Si el faldón empuja contra la pista, esa fuerza no llega a las
  ruedas y además frena. Por eso las aletas no tocan el piso (0,5 mm de luz) y van
  inclinadas: si el piso sube, se doblan en vez de levantar el robot. Una pared vertical
  de TPU de 0,45 mm con este perímetro aguantaría varios cientos de gramos antes de
  pandear (estimado con un módulo de ~30 MPa), más que el peso del robot.
- **Rampas de hasta 20°.** En una cresta con quiebre vivo, el piso sube hasta ~8 mm bajo
  la turbina y ~5 mm detrás del eje (suponiendo el apoyo delantero en la regleta, 168 mm
  delante del eje). Lo único rígido debajo de la placa son la bandeja y las cabezas de
  tornillo que ya estaban.
- **Ninguna punta hacia adelante.** Bordes que miran al frente: punta hacia atrás (35°).
  Laterales: punta hacia afuera (20°), así no tocan las tuercas de los soportes de motor.
  Borde trasero: punta hacia atrás (20°). Una aleta con la punta hacia adelante se
  engancha en las uniones de los paneles.
- **Centro de succión cerca del eje.** Lo que carga el apoyo delantero frena. El área
  encerrada por las puntas tiene el centroide ~9 mm delante del eje.

## Antes de imprimir: medir la altura libre

El STL incluido (`faldon_turbina_h8.stl`) es para **8,0 mm** de altura libre, un valor
supuesto. Medí la distancia real entre la cara inferior de la placa y la pista con el
robot apoyado sobre una superficie plana, cerca del agujero de la turbina. Si sacás la
turbina, podés meter la varilla de profundidad del calibre por el agujero desde la cara
superior y restar 1,6 mm del espesor de la placa. Después regenerá el STL:

```bash
openscad --export-format binstl -o faldon_turbina_h7.4.stl -D 'pieza="imprimir"' -D altura_libre=7.4 faldon_turbina.scad
```

Tarda alrededor de un minuto y medio. La probeta se genera igual, con `-D 'pieza="probeta"'`.

## Parámetros

| Parámetro | Valor | Qué controla |
|---|---|---|
| `altura_libre` | 8,0 mm | Placa → pista. **Medirla.** |
| `luz` | 0,5 mm | Punta de aleta → pista. Bajar a 0,3 si la pista es pareja. |
| `espesor_bandeja` | 1,6 mm | Rigidez de la bandeja; baja la altura de aleta. |
| `ancho_aleta` | 0,6 mm | Ancho horizontal de la aleta (una sola línea con boquilla de 0,4). |
| `angulo_frente` | 35° | Inclinación de los bordes que miran adelante. |
| `angulo_lados` | 20° | Inclinación de los laterales, hacia afuera. |
| `angulo_trasero` | 20° | Inclinación del borde trasero, hacia atrás. |
| `paso_capa` | 0,2 mm | Altura de las rebanadas de la aleta; igual a la altura de capa. |
| `pieza` | `imprimir` | `imprimir`, `montada`, `verificacion` (con placa, pines y ruedas) o `probeta`. |

## Impresión

- **Material:** TPU 95A (perfil de TPU de la SV08). PETG no sirve para esta pieza:
  no se pliega en las crestas.
- **Orientación:** la del STL. La cara que apoya contra la placa queda sobre la cama y
  las aletas crecen hacia arriba con voladizos de 20–35°. Sin soportes.
- **Capas:** 0,2 mm, boquilla de 0,4. Con el generador de perímetros Arachne la aleta
  de 0,6 mm sale como una sola línea; revisalo en la vista previa del laminador.
- **Cama:** sobre PEI, el TPU se pega mucho; usá adhesivo en barra como desmoldante.
- **Primero la probeta** (`probeta_aletas_h8.stl`): tres aletas de 0,45 / 0,6 / 0,9 mm
  con la inclinación y la altura del faldón. Sirve para ajustar el TPU antes de la pieza
  grande (unos 5 g) y para ver en la mano cuál de los tres espesores se dobla fácil
  sin quedar blanda.

## Montaje

- Se fija con los **2 M2 de la turbina** y los **8 M3 de los soportes de motor**.
  Hacen falta tornillos 2 mm más largos. Poné un tornillo en cada agujero que tenga la
  bandeja: un agujero abierto de la placa dentro del faldón es una fuga.
- Opcional: cinta doble faz fina (no de espuma) en toda la cara de la bandeja. Sella la
  unión con la placa y evita que la bandeja se combe entre tornillos con la succión.
- Las puntas de las aletas sobresalen hasta 1,2 mm del borde de la placa (laterales y
  borde trasero). Quedan a 3,7 mm de la cara interna de las ruedas.
- Nada pegajoso en las puntas: el reglamento prohíbe sustancias de adherencia (prueba de
  la hoja A4). Para la categoría "Carrera" (sin succión) se saca con los mismos tornillos.

## Pruebas sugeridas

1. **Luz:** con el robot sobre una tabla plana, pasá una galga de 0,4 mm por debajo de
   las aletas en todo el contorno. Si toca en algún lado, regenerá con más `luz`.
2. **Rampa:** pegá bolitas de plastilina debajo de la bandeja y de las puntas, y pasá
   despacio por una rampa real. Lo aplastado muestra cuánto sube el piso de verdad.
3. **Depresión:** hacé un agujero de 4 mm en una tabla de melamina de prueba y conectá
   por debajo una manguera a un manómetro de agua en U. Con el robot quieto encima y la
   turbina encendida: 1 mm de columna de agua ≈ 9,8 Pa. La fuerza es depresión × ~33 cm²;
   por ejemplo, 50 mm de agua dan unos 165 g. Hoy el firmware no tiene un modo para
   prender solo la turbina con el robot quieto.
4. **Corriente de la turbina:** con el faldón debería **bajar** (un ventilador centrífugo
   consume menos cuando mueve menos aire). El límite de PWM 128 del firmware sigue igual.

## Verificación hecha sobre el STL (altura libre 8 mm)

Se cortó el STL montado a varias alturas y se comparó con el contorno de la placa, los
pads THT y los tornillos de KiCad:

| Control | Resultado |
|---|---|
| Bandeja dentro de la placa | Sí, 0,89 mm de margen mínimo |
| Pin THT más cercano (J2) | 1,05 mm del borde de la bandeja |
| Agujero de la turbina | Libre (bandeja Ø33,7 contra Ø33,1 de la placa) |
| Cabezas/tuercas (M2 r 2,4, M3 r 3,2) a aleta | ≥ 0,43 mm |
| Ancho de aleta | 0,60 mm en toda la altura |
| Luz a la pista | 0,5 mm |
| Área encerrada por las puntas | ~33 cm², centroide ~9 mm delante del eje, perímetro ~249 mm |
| Masa estimada (TPU 1,21 g/cm³) | ~5,3 g |

No se verificó: que los pines de J2, J8, J3 y SW1 asomen menos de 1,6 mm por debajo de la
placa (con el largo normal de un pin header asoman ~1,4 mm), ni el comportamiento real
en pista.
