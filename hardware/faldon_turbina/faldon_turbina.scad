// Faldón de turbina para Zopapa (placa Rapiduino V1.0, turbina 8520 de 55 mm)
//
// Una sola pieza de TPU 95A: una bandeja plana que se atornilla debajo de la
// placa principal y aletas finas en todo el perímetro que bajan hasta casi
// tocar la pista. La bandeja hace de techo de la cámara de succión, así que la
// cara inferior de la placa no necesita sellar.
//
// Las aletas están inclinadas para que se plieguen en las crestas de las rampas
// en vez de levantar el robot:
//   - bordes que miran hacia adelante (frente y "hombros"): punta hacia atrás;
//   - laterales: punta hacia afuera (deja libres las cabezas de los M3);
//   - borde trasero: punta hacia atrás.
// Ninguna aleta tiene la punta hacia adelante: se engancharía en las uniones
// de los paneles y se daría vuelta.
//
// Coordenadas internas: las de KiCad (hardware/main_pcb/Rapiduino.kicad_pcb),
// en mm, con x a la derecha e y hacia ATRÁS, vistas desde arriba de la placa.
// Ver README.md para medir, imprimir y montar.

/* [Medidas a ajustar] */
// Distancia de la cara inferior de la placa a la pista, con el robot apoyado (mm). MEDIRLA.
altura_libre = 8.0;
// Luz entre la punta de las aletas y la pista (mm).
luz = 0.5;
// Espesor de la bandeja (mm).
espesor_bandeja = 1.6;
// Ancho horizontal de la aleta, el que ve el laminador en cada capa (mm).
ancho_aleta = 0.6;
// Inclinación desde la vertical de los bordes que miran hacia adelante (°).
angulo_frente = 35;
// Inclinación desde la vertical de los laterales, hacia afuera (°).
angulo_lados = 20;
// Inclinación desde la vertical del borde trasero, hacia atrás (°).
angulo_trasero = 20;
// Altura de cada rebanada de la aleta; conviene igual a la altura de capa (mm).
paso_capa = 0.2;

/* [Salida] */
// "imprimir": boca abajo, lista para laminar. "montada": en posición, con la placa en z=0.
// "verificacion": montada junto a la placa, pines y ruedas. "probeta": tiras de prueba de aleta.
pieza = "imprimir"; // [imprimir, montada, verificacion, probeta]

/* [Hidden] */
$fa = 4;
$fs = 0.3;

// ---- Datos de la placa (KiCad) ----
CX = 122.541844;            // centro del agujero de la turbina
CY = 111.0;
R_AGUJERO = 16.558774;      // Ø33,12 en la placa
HOLGURA_AGUJERO = 0.3;      // la bandeja no debe achicar la boca

M2 = [[103.483, 111.0], [141.6, 110.922]];          // tornillos de la turbina
M3 = [[ 96.292, 125.488], [106.292, 125.488],       // soporte motor izquierdo
      [ 96.292, 147.888], [106.292, 147.888],
      [138.412, 125.488], [148.412, 125.42 ],       // soporte motor derecho
      [138.412, 147.888], [148.412, 147.82 ]];
D_M2 = 2.4;
D_M3 = 3.4;

// Tramo trasero de la placa (entre ruedas): x 89,792..154,912, borde trasero
// y = 153,134, esquinas R6.
PCB_X0 = 89.792;
PCB_X1 = 154.912;
PCB_Y1 = 153.134;
R_ESQ_PCB = 6;
MARGEN = 0.9;               // retiro de la bandeja respecto del borde de la placa

// Contorno de la bandeja. Estos valores salen de las holguras a los pines THT
// que asoman por abajo (A1 hasta y=87,9; J2 hasta x=103,5; J3 desde x=142,8;
// SW1 y J9 hasta y=116,7) y a las cabezas de los tornillos.
Y_FRENTE = 90.0;            // borde delantero del cuello
Y_HOMBRO = 118.6;           // comienzo del tramo ancho trasero
CUELLO_X0 = 104.6;          // cuello alrededor del agujero, simétrico respecto de CX
CUELLO_X1 = 2 * CX - CUELLO_X0;
R_CUELLO = 4;
R_OREJA = 3.4;              // material alrededor de los M2 (tuerca M2: 2,3 mm a las puntas)

EJE_Y = 136.69;             // eje de las ruedas (centro de los agujeros de los soportes)
RUEDA_D = 32;
RUEDA_ANCHO = 20;
TROCHA = 95;                // entre centros de rueda

ALTURA_ALETA = altura_libre - espesor_bandeja - luz;
assert(ALTURA_ALETA > 1, "altura_libre demasiado chica para este espesor de bandeja y luz");

// ---- 2D ----
module rrect(x0, y0, x1, y1, r)
    translate([x0 + r, y0 + r]) offset(r = r) square([x1 - x0 - 2 * r, y1 - y0 - 2 * r]);

// Cierre morfológico: rellena esquinas cóncavas con radio c.
module cierre(c) offset(r = -c) offset(delta = c) children();

// Contorno exterior de la cámara a una profundidad s por debajo de la bandeja.
module contorno(s) {
    tf = tan(angulo_frente);
    ts = tan(angulo_lados);
    tr = tan(angulo_trasero);
    cierre(1.5) union() {
        cierre(2) union() {
            rrect(PCB_X0 + MARGEN - s * ts, Y_HOMBRO + s * tf,
                  PCB_X1 - MARGEN + s * ts, PCB_Y1 - MARGEN + s * tr,
                  R_ESQ_PCB - MARGEN);
            rrect(CUELLO_X0 - s * ts, Y_FRENTE + s * tf,
                  CUELLO_X1 + s * ts, Y_HOMBRO + 15, R_CUELLO);
        }
        for (c = M2) translate(c) circle(r = R_OREJA + s * ts);
    }
}

// ---- 3D en coordenadas KiCad: placa en z=0, la pieza hacia z negativo ----
module bandeja() {
    translate([0, 0, -espesor_bandeja])
        linear_extrude(height = espesor_bandeja)
            difference() {
                contorno(0);
                translate([CX, CY]) circle(r = R_AGUJERO + HOLGURA_AGUJERO);
                for (c = M2) translate(c) circle(d = D_M2);
                for (c = M3) translate(c) circle(d = D_M3);
            }
}

module aletas() {
    n = ceil(ALTURA_ALETA / paso_capa);
    dz = ALTURA_ALETA / n;
    for (i = [0 : n - 1]) {
        s = (i + 0.5) * dz;
        translate([0, 0, -espesor_bandeja - (i + 1) * dz])
            linear_extrude(height = dz + 0.01)
                difference() {
                    contorno(s);
                    offset(delta = -ancho_aleta) contorno(s);
                }
    }
}

module faldon_kicad() {
    union() { bandeja(); aletas(); }
}

// KiCad (y hacia atrás) -> físico visto desde arriba: origen en el centro de la
// turbina, +Y hacia adelante, z=0 en la cara inferior de la placa.
module a_fisico() mirror([0, 1, 0]) translate([-CX, -CY, 0]) children();

// ---- Referencias para la vista de verificación ----
module placa_ref() {
    // Contorno simplificado del tramo cercano (no se usa para fabricar).
    color("ForestGreen", 0.35) linear_extrude(1.6) difference() {
        polygon([[72.82, 61.07], [171.88, 61.07], [171.88, 101.0], [155.5, 117.5],
                 [154.91, 120], [154.91, 147.13], [148.91, 153.13], [95.79, 153.13],
                 [89.79, 147.13], [89.79, 120], [89.2, 117.5], [72.82, 101.0]]);
        translate([CX, CY]) circle(r = R_AGUJERO);
    }
}

module pines_ref() {
    // Pines THT que asoman por debajo cerca del faldón (pads de KiCad).
    pines = [[113.335, 86.845], [115.875, 86.845], [118.415, 86.845], [120.955, 86.845],
             [123.495, 86.845], [126.035, 86.845], [128.575, 86.845], [131.115, 86.845],
             [133.655, 86.845], [136.195, 86.845], [138.735, 86.845],
             [101.596, 104.5], [102.547, 102.145],       // J2
             [ 95.553, 109.359], [ 97.349, 107.563],     // J8
             [ 91.11, 113.804], [ 92.906, 112.008],      // J9
             [143.815, 99.37], [143.815, 96.83],         // J3
             [150.165, 99.365], [150.165, 96.825],       // J1
             [149.927, 115.841], [148.519, 114.432], [147.053, 112.967],
             [153.519, 112.249], [152.111, 110.84], [150.646, 109.375]];  // SW1
    color("Gold") for (p = pines) translate([p[0], p[1], -1.5]) cylinder(d = 2, h = 1.5);
}

module ruedas_ref() {
    color("DimGray", 0.6)
        for (sx = [-1, 1])
            translate([CX + sx * TROCHA / 2 - 0.19, EJE_Y, RUEDA_D / 2 - altura_libre])
                rotate([0, 90, 0]) cylinder(d = RUEDA_D, h = RUEDA_ANCHO, center = true);
}

module pista_ref() {
    color("White", 0.25) translate([70, 55, -altura_libre - 0.5]) cube([105, 105, 0.5]);
}

// ---- Probeta: tres aletas rectas (0,45 / 0,6 / 0,9 mm) con la misma inclinación
// frontal y altura que el faldón, sobre una base del mismo espesor. Se imprime así.
module probeta() {
    anchos = [0.45, 0.6, 0.9];
    largo = 15;
    paso = 12;
    cube([largo + 6, paso * len(anchos) + 2, espesor_bandeja]);
    for (k = [0 : len(anchos) - 1])
        translate([3, 4 + k * paso, espesor_bandeja - 0.01])
            multmatrix([[1, 0, 0, 0], [0, 1, tan(angulo_frente), 0], [0, 0, 1, 0]])
                cube([largo, anchos[k], ALTURA_ALETA + 0.01]);
}

// ---- Salida ----
if (pieza == "imprimir") {
    // Boca abajo: la cara que apoya contra la placa queda sobre la cama y las aletas
    // crecen hacia arriba con voladizos de 20-35°, sin soportes.
    rotate([180, 0, 0]) a_fisico() faldon_kicad();
} else if (pieza == "montada") {
    a_fisico() faldon_kicad();
} else if (pieza == "verificacion") {
    a_fisico() {
        color("OrangeRed") faldon_kicad();
        placa_ref();
        pines_ref();
        ruedas_ref();
        pista_ref();
    }
} else if (pieza == "probeta") {
    probeta();
}

echo(str("Altura de aleta: ", ALTURA_ALETA, " mm"));
