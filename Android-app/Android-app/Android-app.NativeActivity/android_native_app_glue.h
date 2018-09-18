/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Sous licence Apache, version 2.0 (la « Licence ») ;
 * l'utilisation de ce fichier est soumise au respect de la Licence.
 * Vous pouvez obtenir une copie de la Licence à l'adresse suivante :
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Sauf exigence par une loi applicable ou accord écrit, tout logiciel
 * distribué dans le cadre de la Licence est fourni « EN L'ÉTAT »,
 * SANS GARANTIE OU CONDITION D’AUCUNE SORTE, explicite ou implicite.
 * Consultez la Licence pour les dispositions spécifiques régissant les autorisations
 * et limitations dans le cadre de la Licence.
 *
 */

#ifndef _ANDROID_NATIVE_APP_GLUE_H
#define _ANDROID_NATIVE_APP_GLUE_H

#include <poll.h>
#include <pthread.h>
#include <sched.h>

#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * L'interface d'activité native fournie par <android/native_activity.h>
 * est basée sur un ensemble de rappels fournis par l'application qui sont appelés
 * par le thread principal de l'activité quand certains événements se produisent.
 *
 * Cela signifie qu'aucun blocage de rappel ne doit avoir lieu ; sinon
 * le système risque de forcer la fermeture de l'application. Ce modèle
 * de programmation est direct, léger, mais contraignant.
 *
 * La bibliothèque statique 'threaded_native_app' est utilisée pour fournir un modèle
 * d'exécution différent quand l'application peut implémenter sa propre boucle d'événements
 * principale dans un thread différent. Son fonctionnement est le suivant :
 *
 * 1/ L'application doit fournir une fonction nommée « android_main() » qui
 *    est appelée lors de la création de l'activité, dans un nouveau thread
 *    distinct à partir du thread principal de l'activité.
 *
 * 2/ android_main() reçoit un pointeur vers une structure « android_app » valide
 *    qui contient des références à d'autres objets importants, par exemple
 *    l'instance de l'objet ANativeActivity que l'application exécute.
 *
 * 3/ L'objet « android_app » conserve une instance d'ALooper qui est déjà
 *    à l'écoute de deux événements importants :
 *
 *      - Événements de cycle de vie d'activité (par exemple, « pause », « resume »). Voir les déclarations
 *        APP_CMD_XXX ci-dessous.
 *
 *      - Événements d'entrée provenant d'AInputQueue associé à l'activité.
 *
 *    Chacun d'eux correspond à un identificateur ALooper retourné par
 *    ALooper_pollOnce avec les valeurs de LOOPER_ID_MAIN et de LOOPER_ID_INPUT,
 *    respectivement.
 *
 *    Votre application peut utiliser le même ALooper pour écouter d'autres
 *    descripteurs de fichier. Ils peuvent soit être basés sur des rappels, soit retourner
 *    des identificateurs à compter de LOOPER_ID_USER.
 *
 * 4/ Chaque fois que vous recevez un événement LOOPER_ID_MAIN ou LOOPER_ID_INPUT,
 *    les données retournées pointent vers une structure android_poll_source.  Vous
 *    pouvez appeler la fonction process() sur celle-ci, puis renseigner les fonctions android_app->onAppCmd
 *    et android_app->onInputEvent à appeler pour votre propre traitement
 *    de l'événement.
 *
 *    Vous pouvez aussi appeler les fonctions de bas niveau pour lire et traiter
 *    les données directement... Examinez les implémentations process_cmd() et
 *    process_input() dans le collage pour voir comment faire.
 *
 * Consultez l'exemple nommé « native-activity » fourni avec le NDK pour obtenir un
 * exemple d'utilisation complet. Consultez également NativeActivity dans JavaDoc.
 */

struct android_app;

/**
 * Données associées à un fd ALooper qui sont retournées en tant que « outData »
 * quand les données de cette source sont prêtes.
 */
struct android_poll_source {
    // Identificateur de cette source. Il peut s'agir de LOOPER_ID_MAIN ou de
    // LOOPER_ID_INPUT.
    int32_t id;

    // Android_app auquel cet identificateur est associé.
    struct android_app* app;

    // Fonction à appeler pour effectuer le traitement standard des données à partir de
    // cette source.
    void (*process)(struct android_app* app, struct android_poll_source* source);
};

/**
 * Il s'agit de l'interface du code de collage standard d'une application
 * à threads. Dans ce modèle, le code de l'application s'exécute
 * dans son propre thread, séparément du thread principal du processus.
 * Il n'est pas nécessaire que ce thread soit associé à la machine
 * virtuelle Java, bien qu'il doive l'être pour effectuer des appels JNI à des
 * objets Java.
 */
struct android_app {
    // L'application peut placer un pointeur vers son propre objet d'état
    // si nécessaire.
    void* userData;

    // Renseignez ceci avec la fonction pour traiter les commandes de l'application principale (APP_CMD_*)
    void (*onAppCmd)(struct android_app* app, int32_t cmd);

    // Renseignez ceci avec la fonction pour traiter les événements d'entrée. À ce stade
    // l'événement a déjà été prédistribué, et il est terminé à
    // son retour. Retourne 1 si vous avez traité l'événement, 0 pour toute distribution
    // par défaut.
    int32_t (*onInputEvent)(struct android_app* app, AInputEvent* event);

    // Instance de l'objet ANativeActivity dans laquelle cette application s'exécute.
    ANativeActivity* activity;

    // Configuration actuelle dans laquelle l'application s'exécute.
    AConfiguration* config;

    // État enregistré de la dernière instance, tel que fourni au moment de la création.
    // Sa valeur est NULL si aucun état n'existe. Vous pouvez l'utiliser en fonction de vos besoins ; la
    // mémoire est conservée jusqu'à l'appel d'android_app_exec_cmd() pour
    // APP_CMD_RESUME, après quoi il est libéré et savedState a la valeur NULL.
    // Ces variables ne doivent être modifiées que lors du traitement d'APP_CMD_SAVE_STATE,
    // après quoi elles sont initialisées à la valeur NULL. Vous pouvez alors allouer de la mémoire (malloc) à votre
    // état et placer les informations ici. Dans ce cas, la mémoire est
    // libérée pour vous plus tard.
    void* savedState;
    size_t savedStateSize;

    // ALooper associé au thread de l'application.
    ALooper* looper;

    // Quand sa valeur n'est pas NULL, il s'agit de la file d'attente d'entrée à partir de laquelle l'application
    // reçoit des événements d'entrée de l'utilisateur.
    AInputQueue* inputQueue;

    // Quand sa valeur n'est pas NULL, il s'agit de la surface de la fenêtre sur laquelle l'application peut dessiner.
    ANativeWindow* window;

    // Rectangle de contenu actuel de la fenêtre. Il s'agit de la zone où le
    // contenu de la fenêtre doit être placé pour qu'il soit visible par l'utilisateur.
    ARect contentRect;

    // État actuel de l'activité de l'application. Il peut avoir la valeur APP_CMD_START,
    // APP_CMD_RESUME, APP_CMD_PAUSE ou APP_CMD_STOP ; voir ci-dessous.
    int activityState;

    // Valeur différente de zéro quand la classe NativeActivity de l'application
    // est détruite et en attente de la fin du thread de l'application.
    int destroyRequested;

    // -------------------------------------------------
    // Vous trouverez ci-dessous une implémentation « privée » du code de collage.

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int msgread;
    int msgwrite;

    pthread_t thread;

    struct android_poll_source cmdPollSource;
    struct android_poll_source inputPollSource;

    int running;
    int stateSaved;
    int destroyed;
    int redrawNeeded;
    AInputQueue* pendingInputQueue;
    ANativeWindow* pendingWindow;
    ARect pendingContentRect;
};

enum {
    /**
     * ID de données Looper de commandes provenant du thread principal de l'application, qui
     * est retourné sous la forme d'un identificateur de l'objet ALooper_pollOnce(). Les données de cet
     * identificateur sont un pointeur vers une structure android_poll_source.
     * Celles-ci peuvent être récupérées et traitées avec android_app_read_cmd()
     * et android_app_exec_cmd().
     */
    LOOPER_ID_MAIN = 1,

    /**
     * ID de données Looper d'événements provenant de l'objet AInputQueue de la
     * fenêtre de l'application, qui est retourné sous la forme d'un identificateur
     * de l'objet ALooper_pollOnce(). Les données de cet identificateur sont un pointeur vers une structure 
     * android_poll_source. Celles-ci peuvent être lues via l'objet
     * inputQueue d'android_app.
     */
    LOOPER_ID_INPUT = 2,

    /**
     * Début des identificateurs ALooper définis par l'utilisateur.
     */
    LOOPER_ID_USER = 3,
};

enum {
    /**
     * Commande du thread principal : l'objet AInputQueue a été modifié. Après traitement
     * de cette commande, android_app->inputQueue est mis à jour avec la nouvelle file d'attente
     * (ou la valeur NULL).
     */
    APP_CMD_INPUT_CHANGED,

    /**
     * Commande du thread principal : un nouvel objet ANativeWindow est prêt à être utilisé. Après
     * réception de cette commande, android_app->window contient la surface de la
     * nouvelle fenêtre.
     */
    APP_CMD_INIT_WINDOW,

    /**
     * Commande du thread principal : l'objet ANativeWindow existant doit être
     * terminé. Après réception de cette commande, android_app->window contient
     * encore la fenêtre existante ; après l'appel de la commande android_app_exec_cmd,
     * elle a la valeur NULL.
     */
    APP_CMD_TERM_WINDOW,

    /**
     * Commande du thread principal : l'objet ANativeWindow actuel a été redimensionné.
     * Redessinez-le avec la nouvelle taille.
     */
    APP_CMD_WINDOW_RESIZED,

    /**
     * Commande du thread principal : le système exige que l'objet ANativeWindow actuel
     * soit redessiné. Vous devez redessiner la fenêtre avant de passer ceci à
     * android_app_exec_cmd() afin d'éviter les problèmes de dessin temporaires.
     */
    APP_CMD_WINDOW_REDRAW_NEEDED,

    /**
     * Commande du thread principal : la zone de contenu de la fenêtre a été modifiée,
     * ce qui peut se produire si la fenêtre d'entrée logicielle est affiché ou masquée. Vous pouvez
     * rechercher le nouveau rectangle de contenu dans android_app::contentRect.
     */
    APP_CMD_CONTENT_RECT_CHANGED,

    /**
     * Commande du thread principal : la fenêtre d'activité de l'application a obtenu
     * le focus d'entrée.
     */
    APP_CMD_GAINED_FOCUS,

    /**
     * Commande du thread principal : la fenêtre d'activité de l'application a perdu
     * le focus d'entrée.
     */
    APP_CMD_LOST_FOCUS,

    /**
     * Commande du thread principal : la configuration actuelle du périphérique a été modifiée.
     */
    APP_CMD_CONFIG_CHANGED,

    /**
     * Commande du thread principal : le système n'a presque plus de mémoire.
     * Essayez de réduire l'utilisation de la mémoire.
     */
    APP_CMD_LOW_MEMORY,

    /**
     * Commande du thread principal : l'activité de l'application a démarré.
     */
    APP_CMD_START,

    /**
     * Commande du thread principal : l'activité de l'application a repris.
     */
    APP_CMD_RESUME,

    /**
     * Commande du thread principal : l'application doit générer un nouvel état enregistré
     * à partir duquel elle pourra être restaurée par la suite si nécessaire. Si vous avez un état enregistré,
     * allouez-le avec malloc et placez-le dans android_app.savedState avec
     * la taille dans android_app.savedStateSize. Il sera libéré pour vous
     * plus tard.
     */
    APP_CMD_SAVE_STATE,

    /**
     * Commande du thread principal : l'activité de l'application est suspendue.
     */
    APP_CMD_PAUSE,

    /**
     * Commande du thread principal : l'activité de l'application a été arrêtée.
     */
    APP_CMD_STOP,

    /**
     * Commande du thread principal : l'activité de l'application est en cours de destruction.
     * L'opération suivante aura lieu après nettoyage et fermeture du thread de l'application.
     */
    APP_CMD_DESTROY,
};

/**
 * Appel quand ALooper_pollAll() retourne LOOPER_ID_MAIN, avec lecture du prochain
 * message de commande de l'application.
 */
int8_t android_app_read_cmd(struct android_app* android_app);

/**
 * Appel avec la commande retournée par android_app_read_cmd() pour effectuer le
 * pré-traitement initial de la commande donnée. Vous pouvez effectuer vos propres
 * actions pour la commande après l'appel de cette fonction.
 */
void android_app_pre_exec_cmd(struct android_app* android_app, int8_t cmd);

/**
 * Appel avec la commande retournée par android_app_read_cmd() pour effectuer le
 * post-traitement final de la commande donnée. Vous devez effectuer vos propres
 * actions pour la commande avant d'appeler cette fonction.
 */
void android_app_post_exec_cmd(struct android_app* android_app, int8_t cmd);

/**
 * Fonction que le code de l'application doit implémenter, représentant
 * l'entrée principale à l'application.
 */
extern void android_main(struct android_app* app);

#ifdef __cplusplus
}
#endif

#endif /* _ANDROID_NATIVE_APP_GLUE_H */
