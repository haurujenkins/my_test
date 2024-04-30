#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/scrnsaver.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SCREEN_OFF_COMMAND "xset dpms force off"
#define SCREEN_ON_COMMAND "xset dpms force on"
#define CHECK_INTERVAL 3
#define KEEP_ALIVE_INTERVAL 300 // 5 minutes en secondes
#define LOCK_INTERVAL (60 * 15) // 1 heure en secondes


// gcc -o mytest mytest.c -lX11 -lXtst -lXext -lXss
// Fonction pour vérifier si une touche est en attente d'être lue
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    // Sauvegarde des paramètres de terminal actuels
    tcgetattr(STDIN_FILENO, &oldt);

    // Activer le mode non canonique pour empêcher le buffer de ligne
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Obtient le statut du fichier d'entrée standard
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    // Change le statut pour activer la lecture non bloquante
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    // Lecture d'un caractère de l'entrée standard (stdin)
    ch = getchar();

    // Restaure les paramètres de terminal initiaux
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    // Restaure le statut du fichier d'entrée standard
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    // Vérifie s'il y a un caractère disponible
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

// Fonction pour garder la session active en simulant une action toutes les 5 minutes
void keep_session_alive() {
    printf("Keeping session alive.\n");

    // Ouvre une connexion vers le serveur X
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        printf("Failed to open X display.\n");
        return;
    }

    // Simule une pression de touche "Shift"
    KeySym shift_key = XK_Shift_L;
    KeyCode shift_code = XKeysymToKeycode(display, shift_key);
    XTestFakeKeyEvent(display, shift_code, True, CurrentTime);
    XTestFakeKeyEvent(display, shift_code, False, CurrentTime);

    // Ferme la connexion vers le serveur X
    XCloseDisplay(display);
    system(SCREEN_OFF_COMMAND);
}

// Fonction pour verrouiller la session
void lock_session() {
    printf("Locking session.\n");
    system("xdg-screensaver lock");

    // Déconnecte l'utilisateur
    pid_t pid = fork();
    if (pid == 0) {
        // Dans le processus enfant, exécute la commande pour tuer tous les processus de l'utilisateur
        execl("/bin/sh", "/bin/sh", "-c", "pkill -KILL -u $(whoami)", (char *)0);
        exit(0); // Termine le processus enfant
    } else if (pid < 0) {
        printf("Failed to fork.\n");
    } else {
        // Dans le processus parent, attend la fin du processus enfant
        waitpid(pid, NULL, 0);
    }
}

int main() {
    printf("Press 'q' to exit.\n");

    time_t screen_off_time; // Variable pour stocker l'heure à laquelle l'écran s'est éteint
    bool screen_was_on = false; // Variable pour suivre si l'écran était allumé précédemment
    time_t last_keep_alive = time(NULL); // Dernière fois où une action a été effectuée pour garder la session active
    time_t last_lock = time(NULL);

    // Ouvre une connexion vers le serveur X
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        printf("Failed to open X display.\n");
        return 1;
    }

    Window root = DefaultRootWindow(display);

    // On capture tous les événements de la souris
    XGrabPointer(display, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    while (1) {
        // Vérifie si une touche est pressée pour éteindre l'écran
        if (kbhit()) {
            char key_pressed = getchar();
            if (key_pressed != '\e') {
                screen_off_time = time(NULL); // Enregistre l'heure à laquelle l'écran s'est éteint
                screen_was_on = true;
                printf("Screen was turned off at: %s", asctime(localtime(&screen_off_time)));
                system(SCREEN_OFF_COMMAND);
            } else {
                screen_off_time = time(NULL); // Enregistre l'heure à laquelle l'écran s'est éteint
                screen_was_on = true;
                printf("Screen was turned on at: %s", asctime(localtime(&screen_off_time)));
                break;
            }
        }

        // Vérifie les événements de mouvement et de clic de souris
        XEvent event;
        if (XCheckTypedEvent(display, MotionNotify, &event) || XCheckTypedEvent(display, ButtonPress, &event)) {
            screen_off_time = time(NULL); // Enregistre l'heure à laquelle l'écran s'est éteint
            screen_was_on = true;
            printf("Screen was turned off at: %s", asctime(localtime(&screen_off_time)));
            system(SCREEN_OFF_COMMAND); // Éteint l'écran à chaque mouvement ou clic de souris
        }

        // Vérifie si 5 minutes se sont écoulées depuis la dernière action pour garder la session active
        time_t current_time = time(NULL);
        if (current_time - last_keep_alive >= KEEP_ALIVE_INTERVAL) {
            keep_session_alive(); // Garde la session active
            last_keep_alive = current_time; // Met à jour le temps de la dernière action
        }
        if (current_time - last_lock >= LOCK_INTERVAL) {
            lock_session();
            exit(0);
        }
    }

    // Libère la capture de la souris
    XUngrabPointer(display, CurrentTime);

    // Ferme la connexion vers le serveur X
    XCloseDisplay(display);

    return 0;
}
