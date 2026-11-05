# Faldón de turbina

Pieza de TPU que cierra el espacio entre la placa principal (Rapiduino V1.0) y
la pista alrededor de la turbina 8520 de 55 mm, para que la turbina genere
depresión en vez de solo mover aire. Es una sola pieza: una **bandeja** plana
de 1,6 mm que se atornilla debajo de la placa y **aletas** de 0,6 mm en todo el
perímetro que bajan hasta 0,5 mm de la pista. La placa no está paralela al piso: la
regleta apoya más bajo que las ruedas y la nariz queda abajo. Por eso las aletas son más
cortas adelante que atrás y sus puntas siguen el plano de la pista.

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
  encerrada por las puntas tiene el centroide ~9 mm delante del eje, así que los precintos
  de la regleta cargan solo ~5 % de la succión (9 mm / 168 mm) y las ruedas el resto.
- **Boca paralela al piso.** La fuerza neta de la depresión es perpendicular al plano de
  las puntas. Si las puntas siguieran la placa inclinada, la succión tendría una componente
  hacia atrás (sen 2,5° ≈ 4 % de la fuerza) que frena todo el tiempo.

## Altura libre e inclinación

Se miden dos alturas, de la pista a la **cara superior** de la placa, con el robot sobre
una superficie plana: en el borde delantero y en el trasero (92 mm entre sí). El modelo
resta el espesor de la placa y calcula la inclinación.

Medido el 2026-11-05 (los STL incluidos, `*_d7_t11.stl`, son para estos valores):

| Punto | Turbina apagada | Apretando la turbina (succión simulada) |
|---|---|---|
| Borde delantero | 8 mm | **7 mm** |
| Borde trasero | 11 mm | **11 mm** |

Se usa el caso apretado, que es el de carrera: inclinación de 2,5°. La cara inferior de la
placa queda a 6,7 mm del piso en el frente del faldón, 7,6 mm bajo la turbina y 9,4 mm
atrás. Las aletas miden de 4,6 mm (adelante) a 7,5 mm (atrás).

- **Con la turbina apagada** la nariz sube 1 mm y la luz adelante pasa a ~1,2 mm. Solo
  afecta al arranque: con la succión la nariz baja y el faldón cierra.
- **La placa gira sobre las ruedas**, así que la altura la fija el apoyo delantero. Si
  cambiás o se gastan los precintos, o si la succión real aplasta más que tu dedo, hay que
  volver a medir. Si la galga marca roce adelante, regenerá con una altura delantera mayor.

Para regenerar con otras medidas:

```bash
openscad --export-format binstl -o faldon_turbina_d7_t11.stl -D 'pieza="imprimir"' -D altura_borde_delantero=7 -D altura_borde_trasero=11 faldon_turbina.scad
```

Tarda unos dos minutos. La probeta se genera igual, con `-D 'pieza="probeta"'`.

## Parámetros

| Parámetro | Valor | Qué controla |
|---|---|---|
| `altura_borde_delantero` | 7,0 mm | Pista → cara superior de la placa, borde delantero, con succión. **Medirla.** |
| `altura_borde_trasero` | 11,0 mm | Lo mismo en el borde trasero. **Medirla.** |
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
- **Primero la probeta** (`probeta_aletas_d7_t11.stl`): tres aletas de 0,45 / 0,6 / 0,9 mm
  con la inclinación del frente y la altura que tiene la aleta bajo la turbina (5,5 mm). Sirve para ajustar el TPU antes de la pieza
  grande (unos 5 g) y para ver en la mano cuál de los tres espesores se dobla fácil
  sin quedar blanda.

## Montaje

- Se fija con los **2 M2 de la turbina** y los **8 M3 de los soportes de motor**.
  Hacen falta tornillos 2 mm más largos. Poné un tornillo en cada agujero que tenga la
  bandeja: un agujero abierto de la placa dentro del faldón es una fuga.
- Opcional: cinta doble faz fina (no de espuma) en toda la cara de la bandeja. Sella la
  unión con la placa y evita que la bandeja se combe entre tornillos con la succión.
- Las puntas de las aletas sobresalen hasta 1,8 mm del borde de la placa (laterales y
  borde trasero, donde las aletas son más largas). Quedan a 3,7 mm de la cara interna de las ruedas.
- Nada pegajoso en las puntas: el reglamento prohíbe sustancias de adherencia (prueba de
  la hoja A4). Para la categoría "Carrera" (sin succión) se saca con los mismos tornillos.

## Pruebas sugeridas

1. **Luz:** con el robot sobre una tabla plana y apretando la turbina como al medir,
   pasá una galga de 0,4 mm por debajo de las aletas en todo el contorno. Si toca en algún
   lado, regenerá con más `luz` o con la altura de ese borde corregida.
2. **Rampa:** pegá bolitas de plastilina debajo de la bandeja y de las puntas, y pasá
   despacio por una rampa real. Lo aplastado muestra cuánto sube el piso de verdad.
3. **Depresión:** hacé un agujero de 4 mm en una tabla de melamina de prueba y conectá
   por debajo una manguera a un manómetro de agua en U. Con el robot quieto encima y la
   turbina encendida: 1 mm de columna de agua ≈ 9,8 Pa. La fuerza es depresión × ~33 cm²;
   por ejemplo, 50 mm de agua dan unos 165 g. Hoy el firmware no tiene un modo para
   prender solo la turbina con el robot quieto.
4. **Corriente de la turbina:** con el faldón debería **bajar** (un ventilador centrífugo
   consume menos cuando mueve menos aire). El límite de PWM 128 del firmware sigue igual.

## Verificación hecha sobre el STL

La bandeja y los contornos se verificaron con la versión de 8 mm sin inclinación (la
inclinación solo cambia el largo de las aletas). Se cortó el STL montado a varias alturas y se comparó con el contorno de la placa, los
pads THT y los tornillos de KiCad:

| Control | Resultado |
|---|---|
| Bandeja dentro de la placa | Sí, 0,89 mm de margen mínimo |
| Pin THT más cercano (J2) | 1,05 mm del borde de la bandeja |
| Agujero de la turbina | Libre (bandeja Ø33,7 contra Ø33,1 de la placa) |
| Cabezas/tuercas (M2 r 2,4, M3 r 3,2) a aleta | ≥ 0,43 mm |
| Ancho de aleta | 0,60 mm en toda la altura |
| Luz a la pista inclinada (7 / 11 mm) | 0,5 mm en todo el contorno |
| Área encerrada por las puntas | ~33 cm², centroide ~9 mm delante del eje, perímetro ~249 mm |
| Masa estimada (TPU 1,21 g/cm³) | ~5,3 g |

No se verificó: que los pines de J2, J8, J3 y SW1 asomen menos de 1,6 mm por debajo de la
placa (con el largo normal de un pin header asoman ~1,4 mm), ni el comportamiento real
en pista.
