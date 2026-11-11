#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_CARTAS 52
#define TAM_MANO 8
#define MAX_JUGADA 5
#define MAX_COMODINES 5
#define NUM_RONDAS 3
#define MANOS_POR_RONDA 4
#define DESCARTES_POR_RONDA 3
#define DINERO_INICIAL 4
#define COSTO_TRAGAMONEDAS 3
/* valor: 2-10 normales, 11=J, 12=Q, 13=K, 14=A
   palo: 0=Corazones, 1=Diamantes, 2=Treboles, 3=Picas */

typedef struct {
    int valor;
    int palo;
} Carta;

typedef enum {
    CARTA_ALTA,
    PAREJA,
    DOBLE_PAREJA,
    TRIO,
    ESCALERA,
    COLOR,
    FULL,
    POKER,
    ESCALERA_COLOR
} TipoMano;

enum {
    C_BASICO,
    C_FICHAS,
    C_CORAZON,
    C_PAREJA,
    C_ESCALERA,
    C_REALEZA,
    C_COLOR,
    C_DOBLE,
    C_TRIO,
    C_PICAS,
    C_SOLITARIO,
    C_BARON,      /* usa las cartas que quedan en la mano */
    C_CASTILLO,   /* acumula fichas al descartar */
    C_SIN_CARA,   /* da dinero al descartar */
    C_BANQUERO,   /* da dinero al terminar la ronda */
    C_HUEVO,      /* da cada ronda mas dinero */
    NUM_TIPOS_COMODIN
};

typedef struct {
    const char *nombre;
    const char *descripcion;
    int precio;
} Comodin;

typedef struct {
    int dinero;
    int comodines[MAX_COMODINES]; /* ids de los comodines que tiene */
    int estado[MAX_COMODINES];    /* "memoria" de cada comodin (los que acumulan) */
    int num_comodines;
} Jugador;

const char *NOMBRES_PALOS[] = {"Corazones", "Diamantes", "Treboles", "Picas"};

const char *NOMBRES_MANOS[] = {
    "Carta alta", "Pareja", "Doble pareja", "Trio", "Escalera",
    "Color", "Full", "Poker", "Escalera de color"
};

const int FICHAS_BASE[] = {5, 10, 20, 30, 30, 35, 40, 60, 100};
const int MULT_BASE[]   = {1,  2,  2,  3,  4,  4,  4,  7,   8};

/* Puntos que hay que lograr en cada Ciega*/
const int METAS[NUM_RONDAS] = {450, 1000, 1800};

const Comodin CATALOGO[NUM_TIPOS_COMODIN] = {
    {"Comodin Basico",    "+4 mult",                                     3},
    {"Comodin Fichas",    "+30 fichas",                                  3},
    {"Comodin Corazon",   "+3 mult por cada Corazon que puntue",         5},
    {"Comodin Pareja",    "+8 mult si formas Pareja o Doble pareja",     4},
    {"Comodin Escalera",  "+12 mult si formas Escalera",                 5},
    {"Comodin Realeza",   "+15 fichas por cada J, Q o K que puntue",     5},
    {"Comodin Color",     "+10 mult si formas Color",                    5},
    {"Comodin Doble",     "x2 mult (cuenta lo acumulado hasta ahi)",     8},
    {"Comodin Trio",      "+10 mult si formas Trio",                     4},
    {"Comodin Picas",     "+3 mult por cada Pica que puntue",            5},
    {"Comodin Solitario", "+10 mult si juegas 3 cartas o menos",         4},
    {"Comodin Baron",     "+5 mult por cada K que te quede en la mano",  6},
    {"Comodin Castillo",  "+3 fichas permanentes por cada Pica que descartes", 5},
    {"Comodin Sin Cara",  "Ganas $5 si descartas 3 o mas figuras (J, Q, K)",   5},
    {"Comodin Banquero",  "Ganas $3 al terminar cada ronda",             6},
    {"Comodin Huevo",     "Al terminar cada ronda paga $1 mas que la anterior", 4}
};

/* ========================= JEFES ========================= */

/* La ultima ronda es contra un jefe, que cambia las reglas */
typedef enum {
    JEFE_GANCHO,     /* tras cada mano jugada pierdes 2 cartas al azar */
    JEFE_AGUA,       /* no hay descartes */
    JEFE_EMBRUJO,    /* un palo al azar no puntua */
    JEFE_PSIQUICO,   /* solo se pueden jugar manos de 5 cartas */
    JEFE_OJO,        /* no se puede repetir un tipo de mano */
    NUM_JEFES
} TipoJefe;

typedef struct {
    const char *nombre;
    const char *regla;
} Jefe;

const Jefe JEFES[NUM_JEFES] = {
    {"El Gancho",   "Despues de cada mano que juegues, pierdes 2 cartas al azar de tu mano."},
    {"El Agua",     "No tienes descartes."},
    {"El Embrujo",  "Las cartas de un palo (al azar) no puntuan ni activan comodines."},
    {"El Psiquico", "Solo puedes jugar manos de exactamente 5 cartas."},
    {"El Ojo",      "No puedes repetir el mismo tipo de mano en esta ronda."}
};

/* Llena la baraja con las 52 cartas en orden */
void crear_baraja(Carta baraja[]) {
    int k = 0;
    for (int palo = 0; palo < 4; palo++) {
        for (int valor = 2; valor <= 14; valor++) {
            baraja[k].valor = valor;
            baraja[k].palo = palo;
            k++;
        }
    }
}

void barajar(Carta baraja[]) {
    for (int i = NUM_CARTAS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Carta temp = baraja[i];
        baraja[i] = baraja[j];
        baraja[j] = temp;
    }
}

/* Imprime una carta, por ejemplo: "K de Picas" */
void imprimir_carta(Carta c) {
    switch (c.valor) {
        case 11: printf("J"); break;
        case 12: printf("Q"); break;
        case 13: printf("K"); break;
        case 14: printf("A"); break;
        default: printf("%d", c.valor);
    }
    printf(" de %s", NOMBRES_PALOS[c.palo]);
}

void repartir(Carta baraja[], int *tope, Carta mano[], int n) {
    for (int i = 0; i < n; i++) {
        mano[i] = baraja[*tope];
        (*tope)++;
    }
}

void mostrar_mano(Carta mano[], int n) {
    for (int i = 0; i < n; i++) {
        printf("%d) ", i + 1);
        imprimir_carta(mano[i]);
        printf("\n");
    }
}

Carta robar(Carta baraja[], int *tope) {
    if (*tope >= NUM_CARTAS) {
        crear_baraja(baraja);
        barajar(baraja);
        *tope = 0;
    }
    return baraja[(*tope)++];
}

int comparar_cartas(const void *a, const void *b) {
    const Carta *x = (const Carta *)a;
    const Carta *y = (const Carta *)b;
    if (x->valor != y->valor) return x->valor - y->valor;
    return x->palo - y->palo;
}

/* Detecta que tipo de mano forman las n cartas jugadas (1 a 5) */
TipoMano evaluar_mano(Carta jugada[], int n) {
    int cuenta[15] = {0};   /* cuantas cartas hay de cada valor (2..14) */
    int palos[4] = {0};     /* cuantas cartas hay de cada palo */
    int pares = 0, trios = 0, pokers = 0;

    for (int i = 0; i < n; i++) {
        cuenta[jugada[i].valor]++;
        palos[jugada[i].palo]++;
    }

    for (int v = 2; v <= 14; v++) {
        if (cuenta[v] == 2) pares++;
        if (cuenta[v] == 3) trios++;
        if (cuenta[v] == 4) pokers++;
    }

    /* Color y escalera solo existen con 5 cartas */
    int color = 0;
    int escalera = 0;
    if (n == 5) {
        for (int p = 0; p < 4; p++) {
            if (palos[p] == 5) color = 1;
        }

        /* 5 valores seguidos, de start hasta start+4 */
        for (int start = 2; start <= 10; start++) {
            int seguidas = 1;
            for (int k = 0; k < 5; k++) {
                if (cuenta[start + k] != 1) seguidas = 0;
            }
            if (seguidas) escalera = 1;
        }
        /* Escalera baja: A, 2, 3, 4, 5 */
        if (cuenta[14] == 1 && cuenta[2] == 1 && cuenta[3] == 1 &&
            cuenta[4] == 1 && cuenta[5] == 1) {
            escalera = 1;
        }
    }

    if (escalera && color)        return ESCALERA_COLOR;
    if (pokers == 1)              return POKER;
    if (trios == 1 && pares == 1) return FULL;
    if (color)                    return COLOR;
    if (escalera)                 return ESCALERA;
    if (trios == 1)               return TRIO;
    if (pares == 2)               return DOBLE_PAREJA;
    if (pares == 1)               return PAREJA;
    return CARTA_ALTA;
}

int leer_entero(int *x) {
    int r = scanf("%d", x);
    if (r == 1) return 1;
    if (r == EOF) {            /* se cerro la entrada (Ctrl+D / Ctrl+Z) */
        printf("\nEntrada terminada. Hasta luego.\n");
        exit(0);
    }
    /* Entrada invalida: descartar lo que quedo en el buffer */
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
    return 0;
}

/* Pide un numero entre min y max, repitiendo hasta que sea valido */
int pedir_opcion(int min, int max) {
    int x;
    while (!leer_entero(&x) || x < min || x > max) {
        printf("Opcion invalida. Elige entre %d y %d: ", min, max);
    }
    return x;
}

int elegir_cartas(Carta mano[], int n_mano, Carta jugada[], int indices[]) {
    int cantidad;

    printf("\nCuantas cartas (1-%d, 0 para cancelar)? ", MAX_JUGADA);
    cantidad = pedir_opcion(0, MAX_JUGADA);
    if (cantidad == 0) return 0;

    int usada[TAM_MANO] = {0};
    for (int i = 0; i < cantidad; i++) {
        int pos;
        printf("Carta %d de %d (numero 1-%d): ", i + 1, cantidad, n_mano);
        while (!leer_entero(&pos) || pos < 1 || pos > n_mano || usada[pos - 1]) {
            printf("Invalida o repetida. Elige otra (1-%d): ", n_mano);
        }
        usada[pos - 1] = 1;
        jugada[i] = mano[pos - 1];
        indices[i] = pos - 1;
    }
    return cantidad;
}

int fichas_carta(Carta c) {
    if (c.valor == 14) return 11;
    if (c.valor >= 11) return 10;
    return c.valor;
}

void marcar_puntuan(Carta jugada[], int n, TipoMano tipo, int puntua[]) {
    int cuenta[15] = {0};
    for (int i = 0; i < n; i++) {
        cuenta[jugada[i].valor]++;
        puntua[i] = 0;
    }

    if (tipo == CARTA_ALTA) {
        int mejor = 0;
        for (int i = 1; i < n; i++) {
            if (jugada[i].valor > jugada[mejor].valor) mejor = i;
        }
        puntua[mejor] = 1;
    } else if (tipo == ESCALERA || tipo == COLOR || tipo == ESCALERA_COLOR) {
        for (int i = 0; i < n; i++) puntua[i] = 1;
    } else {
        for (int i = 0; i < n; i++) {
            if (cuenta[jugada[i].valor] >= 2) puntua[i] = 1;
        }
    }
}

//aplica el efecto de un comodin, modificando fichas y mult.
void aplicar_comodin(Jugador *j, int slot, Carta jugada[], int n, int puntua[],
                     Carta resto[], int n_resto, TipoMano tipo,
                     int *fichas, int *mult) {
    int id = j->comodines[slot];
    switch (id) {
        case C_BASICO:
            *mult += 4;
            break;
        case C_FICHAS:
            *fichas += 30;
            break;
        case C_CORAZON:
            for (int i = 0; i < n; i++) {
                if (puntua[i] && jugada[i].palo == 0) *mult += 3;
            }
            break;
        case C_PAREJA:
            if (tipo == PAREJA || tipo == DOBLE_PAREJA) *mult += 8;
            break;
        case C_ESCALERA:
            if (tipo == ESCALERA || tipo == ESCALERA_COLOR) *mult += 12;
            break;
        case C_REALEZA:
            for (int i = 0; i < n; i++) {
                if (puntua[i] && jugada[i].valor >= 11 && jugada[i].valor <= 13) {
                    *fichas += 15;
                }
            }
            break;
        case C_COLOR:
            if (tipo == COLOR || tipo == ESCALERA_COLOR) *mult += 10;
            break;
        case C_DOBLE:
            *mult *= 2;
            break;
        case C_TRIO:
            if (tipo == TRIO) *mult += 10;
            break;
        case C_PICAS:
            for (int i = 0; i < n; i++) {
                if (puntua[i] && jugada[i].palo == 3) *mult += 3;
            }
            break;
        case C_SOLITARIO:
            if (n <= 3) *mult += 10;
            break;
        case C_BARON:   /* cuenta los K que NO jugaste */
            for (int i = 0; i < n_resto; i++) {
                if (resto[i].valor == 13) *mult += 5;
            }
            break;
        case C_CASTILLO: /* suma lo que acumulo al descartar Picas */
            *fichas += j->estado[slot];
            break;
    }
}

int calcular_puntaje(Jugador *j, Carta jugada[], int n, Carta resto[], int n_resto,
                    TipoMano tipo, int palo_debil, int mostrar) {
    int puntua[MAX_JUGADA];
    marcar_puntuan(jugada, n, tipo, puntua);

    int debil[MAX_JUGADA] = {0};
    for (int i = 0; i < n; i++) {
        if (palo_debil >= 0 && jugada[i].palo == palo_debil && puntua[i]) {
            puntua[i] = 0;
            debil[i] = 1;
        }
    }

    int fichas = FICHAS_BASE[tipo];
    int mult = MULT_BASE[tipo];

    if (mostrar) {
        printf("\n>> %s: %d fichas x %d mult\n", NOMBRES_MANOS[tipo], fichas, mult);
    }

    for (int i = 0; i < n; i++) {
        if (debil[i]) {
            if (mostrar) {
                printf("   ");
                imprimir_carta(jugada[i]);
                printf(": anulada por el jefe\n");
            }
        } else if (puntua[i]) {
            fichas += fichas_carta(jugada[i]);
            if (mostrar) {
                printf("   ");
                imprimir_carta(jugada[i]);
                printf(": +%d fichas\n", fichas_carta(jugada[i]));
            }
        }
    }

    for (int i = 0; i < j->num_comodines; i++) {
        int f0 = fichas, m0 = mult;
        aplicar_comodin(j, i, jugada, n, puntua, resto, n_resto, tipo, &fichas, &mult);
        if (mostrar && (fichas != f0 || mult != m0)) {
            printf("   %s: fichas %d, mult %d\n",
                   CATALOGO[j->comodines[i]].nombre, fichas, mult);
        }
    }

    int total = fichas * mult;
    if (mostrar) printf("   TOTAL: %d x %d = %d puntos\n", fichas, mult, total);
    return total;
}

void comodines_al_descartar(Jugador *j, Carta descartadas[], int n) {
    for (int s = 0; s < j->num_comodines; s++) {
        switch (j->comodines[s]) {
            case C_CASTILLO: {
                int picas = 0;
                for (int i = 0; i < n; i++) {
                    if (descartadas[i].palo == 3) picas++;
                }
                if (picas > 0) {
                    j->estado[s] += 3 * picas;
                    printf("   %s: +%d fichas permanentes (ya acumula %d)\n",
                           CATALOGO[C_CASTILLO].nombre, 3 * picas, j->estado[s]);
                }
                break;
            }
            case C_SIN_CARA: {
                int figuras = 0;
                for (int i = 0; i < n; i++) {
                    if (descartadas[i].valor >= 11 && descartadas[i].valor <= 13) figuras++;
                }
                if (figuras >= 3) {
                    j->dinero += 5;
                    printf("   %s: +$5\n", CATALOGO[C_SIN_CARA].nombre);
                }
                break;
            }
        }
    }
}

void comodines_fin_ronda(Jugador *j) {
    for (int s = 0; s < j->num_comodines; s++) {
        switch (j->comodines[s]) {
            case C_BANQUERO:
                j->dinero += 3;
                printf("   %s: +$3\n", CATALOGO[C_BANQUERO].nombre);
                break;
            case C_HUEVO:
                j->estado[s]++;                 /* cada ronda paga uno mas */
                j->dinero += j->estado[s];
                printf("   %s: +$%d\n", CATALOGO[C_HUEVO].nombre, j->estado[s]);
                break;
        }
    }
}

void mostrar_comodines(Jugador *j) {
    printf("Comodines (%d/%d): ", j->num_comodines, MAX_COMODINES);
    if (j->num_comodines == 0) printf("ninguno");
    for (int i = 0; i < j->num_comodines; i++) {
        printf("[%s] ", CATALOGO[j->comodines[i]].nombre);
    }
    printf("\n");
}

int tiene_comodin(Jugador *j, int id) {
    for (int i = 0; i < j->num_comodines; i++) {
        if (j->comodines[i] == id) return 1;
    }
    return 0;
}

/* Maquina tragamonedas: tres simbolos al azar.
   Tres iguales = $15, dos iguales = $3 (recuperas lo pagado), ninguno = pierdes. */
void tragamonedas(Jugador *j) {
    const char *simbolos[] = {"CEREZA", "LIMON", "SIETE", "BAR"};

    if (j->dinero < COSTO_TRAGAMONEDAS) {
        printf("No te alcanza el dinero para jugar.\n");
        return;
    }
    j->dinero -= COSTO_TRAGAMONEDAS;

    int r[3];
    for (int i = 0; i < 3; i++) r[i] = rand() % 4;
    printf("\n[ %s ] [ %s ] [ %s ]\n", simbolos[r[0]], simbolos[r[1]], simbolos[r[2]]);

    int premio = 0;
    if (r[0] == r[1] && r[1] == r[2]) {
        premio = 15;
        printf("JACKPOT! Ganas $%d\n", premio);
    } else if (r[0] == r[1] || r[1] == r[2] || r[0] == r[2]) {
        premio = 3;
        printf("Dos iguales: recuperas tus $%d\n", premio);
    } else {
        printf("Nada... la casa gana.\n");
    }
    j->dinero += premio;
}

void tienda(Jugador *j) {
    int oferta[3];
    int vendido[3] = {0};

    /* Elegir 3 comodines distintos que el jugador todavia no tenga */
    for (int i = 0; i < 3; i++) {
        int id, repetido;
        do {
            id = rand() % NUM_TIPOS_COMODIN;
            repetido = tiene_comodin(j, id);
            for (int k = 0; k < i; k++) {
                if (oferta[k] == id) repetido = 1;
            }
        } while (repetido);
        oferta[i] = id;
    }

    int opcion;
    do {
        printf("\n=============== TIENDA ===============\n");
        printf("Dinero: $%d\n", j->dinero);
        mostrar_comodines(j);
        for (int i = 0; i < j->num_comodines; i++) {
            printf("   - %s: %s\n", CATALOGO[j->comodines[i]].nombre,
                   CATALOGO[j->comodines[i]].descripcion);
        }

        printf("\nEn venta:\n");
        for (int i = 0; i < 3; i++) {
            if (vendido[i]) {
                printf("%d) (comprado)\n", i + 1);
            } else {
                printf("%d) %s - %s ($%d)\n", i + 1, CATALOGO[oferta[i]].nombre,
                       CATALOGO[oferta[i]].descripcion, CATALOGO[oferta[i]].precio);
            }
        }
        printf("4) Tragamonedas ($%d)\n", COSTO_TRAGAMONEDAS);
        printf("0) Siguiente ronda\n> ");

        opcion = pedir_opcion(0, 4);

        if (opcion >= 1 && opcion <= 3) {
            int k = opcion - 1;
            if (vendido[k]) {
                printf("Ya compraste ese comodin.\n");
            } else if (j->num_comodines >= MAX_COMODINES) {
                printf("No tienes espacio para mas comodines.\n");
            } else if (j->dinero < CATALOGO[oferta[k]].precio) {
                printf("No te alcanza el dinero.\n");
            } else {
                j->dinero -= CATALOGO[oferta[k]].precio;
                j->comodines[j->num_comodines] = oferta[k];
                j->estado[j->num_comodines] = 0;   /* empieza sin acumular nada */
                j->num_comodines++;
                vendido[k] = 1;
                printf("Compraste %s!\n", CATALOGO[oferta[k]].nombre);
            }
        } else if (opcion == 4) {
            tragamonedas(j);
        }
    } while (opcion != 0);
}

int jugar_ronda(Jugador *j, int num_ronda, int meta, int jefe) {
    Carta baraja[NUM_CARTAS];
    Carta mano[TAM_MANO];
    Carta jugada[MAX_JUGADA];
    int indices[MAX_JUGADA];
    int tope = 0;
    int puntos = 0;
    int manos = MANOS_POR_RONDA;
    int descartes = DESCARTES_POR_RONDA;
    int palo_debil = -1;          /* palo anulado por El Embrujo */
    int tipo_usado[9] = {0};      /* tipos de mano ya jugados (El Ojo) */

    if (jefe == JEFE_AGUA) descartes = 0;
    if (jefe == JEFE_EMBRUJO) palo_debil = rand() % 4;

    crear_baraja(baraja);
    barajar(baraja);
    repartir(baraja, &tope, mano, TAM_MANO);
    qsort(mano, TAM_MANO, sizeof(Carta), comparar_cartas);

    printf("\n======================================\n");
    printf("  RONDA %d de %d  -  Meta: %d puntos\n", num_ronda, NUM_RONDAS, meta);
    printf("======================================\n");
    if (jefe >= 0) {
        printf("\n!!! RONDA DE JEFE: %s !!!\n%s\n", JEFES[jefe].nombre, JEFES[jefe].regla);
        if (jefe == JEFE_EMBRUJO) {
            printf("Palo anulado: %s\n", NOMBRES_PALOS[palo_debil]);
        }
    }

    while (puntos < meta && manos > 0) {
        printf("\nPuntos: %d/%d | Manos: %d | Descartes: %d | Dinero: $%d\n",
               puntos, meta, manos, descartes, j->dinero);
        mostrar_comodines(j);
        if (jefe >= 0) {
            printf("JEFE %s: %s", JEFES[jefe].nombre, JEFES[jefe].regla);
            if (jefe == JEFE_EMBRUJO) printf(" (%s)", NOMBRES_PALOS[palo_debil]);
            printf("\n");
        }
        printf("\n--- Tu mano ---\n");
        mostrar_mano(mano, TAM_MANO);

        printf("\n1) Jugar mano\n2) Descartar\n> ");
        int op = pedir_opcion(1, 2);

        if (op == 2 && descartes == 0) {
            printf("No te quedan descartes.\n");
            continue;
        }

        int cantidad = elegir_cartas(mano, TAM_MANO, jugada, indices);
        if (cantidad == 0) continue;   /* cancelo */

        if (op == 1) {
            /* cartas que se quedan en la mano sin jugar (las usa el Baron) */
            int jugada_flag[TAM_MANO] = {0};
            Carta resto[TAM_MANO];
            int n_resto = 0;
            for (int i = 0; i < cantidad; i++) jugada_flag[indices[i]] = 1;
            for (int i = 0; i < TAM_MANO; i++) {
                if (!jugada_flag[i]) resto[n_resto++] = mano[i];
            }

            TipoMano tipo = evaluar_mano(jugada, cantidad);

            if (jefe == JEFE_PSIQUICO && cantidad != MAX_JUGADA) {
                printf("El Psiquico solo permite jugar exactamente %d cartas.\n", MAX_JUGADA);
                continue;
            }
            if (jefe == JEFE_OJO && tipo_usado[tipo]) {
                printf("El Ojo no deja repetir '%s' en esta ronda.\n", NOMBRES_MANOS[tipo]);
                continue;
            }
            tipo_usado[tipo] = 1;

            puntos += calcular_puntaje(j, jugada, cantidad, resto, n_resto, tipo, palo_debil, 1);
            manos--;
        } else {
            descartes--;
            printf("Descartaste %d carta(s).\n", cantidad);
            comodines_al_descartar(j, jugada, cantidad);
        }

        for (int i = 0; i < cantidad; i++) {
            mano[indices[i]] = robar(baraja, &tope);
        }

        if (jefe == JEFE_GANCHO && op == 1 && puntos < meta && manos > 0) {
            int a = rand() % TAM_MANO;
            int b;
            do {
                b = rand() % TAM_MANO;
            } while (b == a);
            printf("\nEl Gancho te arrebata: ");
            imprimir_carta(mano[a]);
            printf(" y ");
            imprimir_carta(mano[b]);
            printf("\n");
            mano[a] = robar(baraja, &tope);
            mano[b] = robar(baraja, &tope);
        }
        qsort(mano, TAM_MANO, sizeof(Carta), comparar_cartas);
    }

    if (puntos >= meta) {
        int premio = 5 + manos;
        j->dinero += premio;
        printf("\n*** RONDA SUPERADA *** (%d/%d puntos)\n", puntos, meta);
        printf("Ganas $%d ($5 por ganar + $1 por cada mano que te sobro)\n", premio);
        comodines_fin_ronda(j);
        return 1;
    }

    printf("Te quedaste sin manos con %d/%d puntos.\n", puntos, meta);
    return 0;
}

void mostrar_reglas(void) {
    printf("=============== VAMOS A BALATREAR EN C ===============\n");
    printf("- Hay %d Ciegas. En cada una debes llegar a la meta de puntos.\n", NUM_RONDAS);
    printf("- Tienes %d manos y %d descartes por ronda.\n", MANOS_POR_RONDA, DESCARTES_POR_RONDA);
    printf("- Cada mano da FICHAS x MULTIPLICADOR. Solo puntuan las cartas\n");
    printf("  que forman la jugada (en una pareja, solo las dos de la pareja).\n");
    printf("- Entre rondas compras comodines (hasta %d) o tientas a la suerte\n", MAX_COMODINES);
    printf("  en el tragamonedas.\n");
    printf("- La ultima ronda es contra una Ciega JEFE que cambia las reglas.\n");
    printf("==================================================\n");
}

int main(void) {
    srand(time(NULL));
    int otra;

    do {
        Jugador j;
        j.dinero = DINERO_INICIAL;
        j.num_comodines = 0;

        mostrar_reglas();

        int ganaste = 1;
        for (int r = 0; r < NUM_RONDAS; r++) {
            int jefe = (r == NUM_RONDAS - 1) ? rand() % NUM_JEFES : -1;
            if (!jugar_ronda(&j, r + 1, METAS[r], jefe)) {
                ganaste = 0;
                break;
            }
            if (r < NUM_RONDAS - 1) tienda(&j);
        }

        if (ganaste) {
            printf("\n*****************************************\n");
            printf("  GANASTE.\n", NUM_RONDAS);
            printf("  Dinero final: $%d\n", j.dinero);
            printf("*****************************************\n");
        } else {
            printf("\n GAME OVER.\n");
        }

        printf("\n Quieres Balatrear de Nuevo? (1 = si, 0 = no, No le sabes a la Balatreada): ");
        otra = pedir_opcion(0, 1);
    } while (otra);

    return 0;
}
