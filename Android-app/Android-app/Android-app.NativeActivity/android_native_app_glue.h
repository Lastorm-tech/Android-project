/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Sous licence Apache, version�2.0 (la ��Licence��)�;
 * l'utilisation de ce fichier est soumise au respect de la Licence.
 * Vous pouvez obtenir une copie de la Licence � l'adresse suivante�:
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Sauf exigence par une loi applicable ou accord �crit, tout logiciel
 * distribu� dans le cadre de la Licence est fourni ��EN L'�TAT��,
 * SANS GARANTIE OU CONDITION D�AUCUNE SORTE, explicite ou implicite.
 * Consultez la Licence pour les dispositions sp�cifiques r�gissant les autorisations
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
 * L'interface d'activit� native fournie par <android/native_activity.h>
 * est bas�e sur un ensemble de rappels fournis par l'application qui sont appel�s
 * par le thread principal de l'activit� quand certains �v�nements se produisent.
 *
 * Cela signifie qu'aucun blocage de rappel ne doit avoir lieu�; sinon
 * le syst�me risque de forcer la fermeture de l'application. Ce mod�le
 * de programmation est direct, l�ger, mais contraignant.
 *
 * La biblioth�que statique 'threaded_native_app' est utilis�e pour fournir un mod�le
 * d'ex�cution diff�rent quand l'application peut impl�menter sa propre boucle d'�v�nements
 * principale dans un thread diff�rent. Son fonctionnement est le suivant�:
 *
 * 1/ L'application doit fournir une fonction nomm�e ��android_main()�� qui
 *    est appel�e lors de la cr�ation de l'activit�, dans un nouveau thread
 *    distinct � partir du thread principal de l'activit�.
 *
 * 2/ android_main() re�oit un pointeur vers une structure ��android_app�� valide
 *    qui contient des r�f�rences � d'autres objets importants, par exemple
 *    l'instance de l'objet ANativeActivity que l'application ex�cute.
 *
 * 3/ L'objet ��android_app�� conserve une instance d'ALooper qui est d�j�
 *    � l'�coute de deux �v�nements importants�:
 *
 *      - �v�nements de cycle de vie d'activit� (par exemple, ��pause��, ��resume��). Voir les d�clarations
 *        APP_CMD_XXX ci-dessous.
 *
 *      - �v�nements d'entr�e provenant d'AInputQueue associ� � l'activit�.
 *
 *    Chacun d'eux correspond � un identificateur ALooper retourn� par
 *    ALooper_pollOnce avec les valeurs de LOOPER_ID_MAIN et de LOOPER_ID_INPUT,
 *    respectivement.
 *
 *    Votre application peut utiliser le m�me ALooper pour �couter d'autres
 *    descripteurs de fichier. Ils peuvent soit �tre bas�s sur des rappels, soit retourner
 *    des identificateurs � compter de LOOPER_ID_USER.
 *
 * 4/ Chaque fois que vous recevez un �v�nement LOOPER_ID_MAIN ou LOOPER_ID_INPUT,
 *    les donn�es retourn�es pointent vers une structure android_poll_source.  Vous
 *    pouvez appeler la fonction process() sur celle-ci, puis renseigner les fonctions android_app->onAppCmd
 *    et android_app->onInputEvent � appeler pour votre propre traitement
 *    de l'�v�nement.
 *
 *    Vous pouvez aussi appeler les fonctions de bas niveau pour lire et traiter
 *    les donn�es directement... Examinez les impl�mentations process_cmd() et
 *    process_input() dans le collage pour voir comment faire.
 *
 * Consultez l'exemple nomm� ��native-activity�� fourni avec le NDK pour obtenir un
 * exemple d'utilisation complet. Consultez �galement NativeActivity dans JavaDoc.
 */

struct android_app;

/**
 * Donn�es associ�es � un fd ALooper qui sont retourn�es en tant que ��outData��
 * quand les donn�es de cette source sont pr�tes.
 */
struct android_poll_source {
    // Identificateur de cette source. Il peut s'agir de LOOPER_ID_MAIN ou de
    // LOOPER_ID_INPUT.
    int32_t id;

    // Android_app auquel cet identificateur est associ�.
    struct android_app* app;

    // Fonction � appeler pour effectuer le traitement standard des donn�es � partir de
    // cette source.
    void (*process)(struct android_app* app, struct android_poll_source* source);
};

/**
 * Il s'agit de l'interface du code de collage standard d'une application
 * � threads. Dans ce mod�le, le code de l'application s'ex�cute
 * dans son propre thread, s�par�ment du thread principal du processus.
 * Il n'est pas n�cessaire que ce thread soit associ� � la machine
 * virtuelle Java, bien qu'il doive l'�tre pour effectuer des appels JNI � des
 * objets Java.
 */
struct android_app {
    // L'application peut placer un pointeur vers son propre objet d'�tat
    // si n�cessaire.
    void* userData;

    // Renseignez ceci avec la fonction pour traiter les commandes de l'application principale (APP_CMD_*)
    void (*onAppCmd)(struct android_app* app, int32_t cmd);

    // Renseignez ceci avec la fonction pour traiter les �v�nements d'entr�e. � ce stade
    // l'�v�nement a d�j� �t� pr�distribu�, et il est termin� �
    // son retour. Retourne�1 si vous avez trait� l'�v�nement, 0�pour toute distribution
    // par d�faut.
    int32_t (*onInputEvent)(struct android_app* app, AInputEvent* event);

    // Instance de l'objet ANativeActivity dans laquelle cette application s'ex�cute.
    ANativeActivity* activity;

    // Configuration actuelle dans laquelle l'application s'ex�cute.
    AConfiguration* config;

    // �tat enregistr� de la derni�re instance, tel que fourni au moment de la cr�ation.
    // Sa valeur est NULL si aucun �tat n'existe. Vous pouvez l'utiliser en fonction de vos besoins�; la
    // m�moire est conserv�e jusqu'� l'appel d'android_app_exec_cmd() pour
    // APP_CMD_RESUME, apr�s quoi il est lib�r� et savedState a la valeur NULL.
    // Ces variables ne doivent �tre modifi�es que lors du traitement d'APP_CMD_SAVE_STATE,
    // apr�s quoi elles sont initialis�es � la valeur NULL. Vous pouvez alors allouer de la m�moire (malloc) � votre
    // �tat et placer les informations ici. Dans ce cas, la m�moire est
    // lib�r�e pour vous plus tard.
    void* savedState;
    size_t savedStateSize;

    // ALooper associ� au thread de l'application.
    ALooper* looper;

    // Quand sa valeur n'est pas NULL, il s'agit de la file d'attente d'entr�e � partir de laquelle l'application
    // re�oit des �v�nements d'entr�e de l'utilisateur.
    AInputQueue* inputQueue;

    // Quand sa valeur n'est pas NULL, il s'agit de la surface de la fen�tre sur laquelle l'application peut dessiner.
    ANativeWindow* window;

    // Rectangle de contenu actuel de la fen�tre. Il s'agit de la zone o� le
    // contenu de la fen�tre doit �tre plac� pour qu'il soit visible par l'utilisateur.
    ARect contentRect;

    // �tat actuel de l'activit� de l'application. Il peut avoir la valeur APP_CMD_START,
    // APP_CMD_RESUME, APP_CMD_PAUSE ou APP_CMD_STOP�; voir ci-dessous.
    int activityState;

    // Valeur diff�rente de z�ro quand la classe NativeActivity de l'application
    // est d�truite et en attente de la fin du thread de l'application.
    int destroyRequested;

    // -------------------------------------------------
    // Vous trouverez ci-dessous une impl�mentation ��priv�e�� du code de collage.

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
     * ID de donn�es Looper de commandes provenant du thread principal de l'application, qui
     * est retourn� sous la forme d'un identificateur de l'objet ALooper_pollOnce(). Les donn�es de cet
     * identificateur sont un pointeur vers une structure android_poll_source.
     * Celles-ci peuvent �tre r�cup�r�es et trait�es avec android_app_read_cmd()
     * et android_app_exec_cmd().
     */
    LOOPER_ID_MAIN = 1,

    /**
     * ID de donn�es Looper d'�v�nements provenant de l'objet AInputQueue de la
     * fen�tre de l'application, qui est retourn� sous la forme d'un identificateur
     * de l'objet ALooper_pollOnce(). Les donn�es de cet identificateur sont un pointeur vers une structure 
     * android_poll_source. Celles-ci peuvent �tre lues via l'objet
     * inputQueue d'android_app.
     */
    LOOPER_ID_INPUT = 2,

    /**
     * D�but des identificateurs ALooper d�finis par l'utilisateur.
     */
    LOOPER_ID_USER = 3,
};

enum {
    /**
     * Commande du thread principal�: l'objet AInputQueue a �t� modifi�. Apr�s traitement
     * de cette commande, android_app->inputQueue est mis � jour avec la nouvelle file d'attente
     * (ou la valeur NULL).
     */
    APP_CMD_INPUT_CHANGED,

    /**
     * Commande du thread principal�: un nouvel objet ANativeWindow est pr�t � �tre utilis�. Apr�s
     * r�ception de cette commande, android_app->window contient la surface de la
     * nouvelle fen�tre.
     */
    APP_CMD_INIT_WINDOW,

    /**
     * Commande du thread principal�: l'objet ANativeWindow existant doit �tre
     * termin�. Apr�s r�ception de cette commande, android_app->window contient
     * encore la fen�tre existante�; apr�s l'appel de la commande android_app_exec_cmd,
     * elle a la valeur NULL.
     */
    APP_CMD_TERM_WINDOW,

    /**
     * Commande du thread principal�: l'objet ANativeWindow actuel a �t� redimensionn�.
     * Redessinez-le avec la nouvelle taille.
     */
    APP_CMD_WINDOW_RESIZED,

    /**
     * Commande du thread principal�: le syst�me exige que l'objet ANativeWindow actuel
     * soit redessin�. Vous devez redessiner la fen�tre avant de passer ceci �
     * android_app_exec_cmd() afin d'�viter les probl�mes de dessin temporaires.
     */
    APP_CMD_WINDOW_REDRAW_NEEDED,

    /**
     * Commande du thread principal�: la zone de contenu de la fen�tre a �t� modifi�e,
     * ce qui peut se produire si la fen�tre d'entr�e logicielle est affich� ou masqu�e. Vous pouvez
     * rechercher le nouveau rectangle de contenu dans android_app::contentRect.
     */
    APP_CMD_CONTENT_RECT_CHANGED,

    /**
     * Commande du thread principal�: la fen�tre d'activit� de l'application a obtenu
     * le focus d'entr�e.
     */
    APP_CMD_GAINED_FOCUS,

    /**
     * Commande du thread principal�: la fen�tre d'activit� de l'application a perdu
     * le focus d'entr�e.
     */
    APP_CMD_LOST_FOCUS,

    /**
     * Commande du thread principal�: la configuration actuelle du p�riph�rique a �t� modifi�e.
     */
    APP_CMD_CONFIG_CHANGED,

    /**
     * Commande du thread principal�: le syst�me n'a presque plus de m�moire.
     * Essayez de r�duire l'utilisation de la m�moire.
     */
    APP_CMD_LOW_MEMORY,

    /**
     * Commande du thread principal�: l'activit� de l'application a d�marr�.
     */
    APP_CMD_START,

    /**
     * Commande du thread principal�: l'activit� de l'application a repris.
     */
    APP_CMD_RESUME,

    /**
     * Commande du thread principal�: l'application doit g�n�rer un nouvel �tat enregistr�
     * � partir duquel elle pourra �tre restaur�e par la suite si n�cessaire. Si vous avez un �tat enregistr�,
     * allouez-le avec malloc et placez-le dans android_app.savedState avec
     * la taille dans android_app.savedStateSize. Il sera lib�r� pour vous
     * plus tard.
     */
    APP_CMD_SAVE_STATE,

    /**
     * Commande du thread principal�: l'activit� de l'application est suspendue.
     */
    APP_CMD_PAUSE,

    /**
     * Commande du thread principal�: l'activit� de l'application a �t� arr�t�e.
     */
    APP_CMD_STOP,

    /**
     * Commande du thread principal�: l'activit� de l'application est en cours de destruction.
     * L'op�ration suivante aura lieu apr�s nettoyage et fermeture du thread de l'application.
     */
    APP_CMD_DESTROY,
};

/**
 * Appel quand ALooper_pollAll() retourne LOOPER_ID_MAIN, avec lecture du prochain
 * message de commande de l'application.
 */
int8_t android_app_read_cmd(struct android_app* android_app);

/**
 * Appel avec la commande retourn�e par android_app_read_cmd() pour effectuer le
 * pr�-traitement initial de la commande donn�e. Vous pouvez effectuer vos propres
 * actions pour la commande apr�s l'appel de cette fonction.
 */
void android_app_pre_exec_cmd(struct android_app* android_app, int8_t cmd);

/**
 * Appel avec la commande retourn�e par android_app_read_cmd() pour effectuer le
 * post-traitement final de la commande donn�e. Vous devez effectuer vos propres
 * actions pour la commande avant d'appeler cette fonction.
 */
void android_app_post_exec_cmd(struct android_app* android_app, int8_t cmd);

/**
 * Fonction que le code de l'application doit impl�menter, repr�sentant
 * l'entr�e principale � l'application.
 */
extern void android_main(struct android_app* app);

#ifdef __cplusplus
}
#endif

#endif /* _ANDROID_NATIVE_APP_GLUE_H */
