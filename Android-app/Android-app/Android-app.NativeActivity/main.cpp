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

// Lastorm tech.

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

/**
* Données d'état enregistrées.
*/
struct saved_state {
	float angle;
	int32_t x;
	int32_t y;
};

/**
* État partagé de l'application.
*/
struct engine {
	struct android_app* app;

	ASensorManager* sensorManager;
	const ASensor* accelerometerSensor;
	ASensorEventQueue* sensorEventQueue;

	int animating;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	int32_t width;
	int32_t height;
	struct saved_state state;
};

/**
* Initialisation d'un contexte EGL pour l'affichage en cours.
*/
static int engine_init_display(struct engine* engine) {
	// Initialisation d'OpenGL ES et EGL

	/*
	* Ici, les attributs de la configuration désirée sont spécifiés.
	* Un composant EGLConfig avec au moins 8 bits par couleur
	* compatible avec les fenêtres à l'écran est sélectionné ci-dessous.
	*/
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLint w, h, format;
	EGLint numConfigs;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;

	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	eglInitialize(display, 0, 0);

	/* Ici, l'application choisit la configuration désirée. Cet
	* exemple illustre un processus de sélection très simplifié dans lequel le premier EGLConfig
	* correspondant aux critères est sélectionné. */
	eglChooseConfig(display, attribs, &config, 1, &numConfigs);

	/* EGL_NATIVE_VISUAL_ID est un attribut d'EGLConfig dont
	* l'acceptation par ANativeWindow_setBuffersGeometry() est garantie.
	* Dès qu'un EGLConfig est choisi, il est possible de reconfigurer en toute sécurité les
	* mémoires tampons ANativeWindow pour les faire correspondre à l'aide d'EGL_NATIVE_VISUAL_ID. */
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
	context = eglCreateContext(display, config, NULL, NULL);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGW("Unable to eglMakeCurrent");
		return -1;
	}

	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);

	engine->display = display;
	engine->context = context;
	engine->surface = surface;
	engine->width = w;
	engine->height = h;
	engine->state.angle = 0;

	// Initialisation de l'état GL.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);

	return 0;
}

/**
* Uniquement l'image actuelle dans l'affichage.
*/
static void engine_draw_frame(struct engine* engine) {
	if (engine->display == NULL) {
		// Aucun affichage.
		return;
	}

	// Remplissage de l'écran avec simplement une couleur.
	glClearColor(((float)engine->state.x) / engine->width, engine->state.angle,
		((float)engine->state.y) / engine->height, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(engine->display, engine->surface);
}

/**
* Destruction du contexte EGL actuellement associé à l'affichage.
*/
static void engine_term_display(struct engine* engine) {
	if (engine->display != EGL_NO_DISPLAY) {
		eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (engine->context != EGL_NO_CONTEXT) {
			eglDestroyContext(engine->display, engine->context);
		}
		if (engine->surface != EGL_NO_SURFACE) {
			eglDestroySurface(engine->display, engine->surface);
		}
		eglTerminate(engine->display);
	}
	engine->animating = 0;
	engine->display = EGL_NO_DISPLAY;
	engine->context = EGL_NO_CONTEXT;
	engine->surface = EGL_NO_SURFACE;
}

/**
* Traitement de l'événement d'entrée suivant.
*/
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	struct engine* engine = (struct engine*)app->userData;
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		engine->state.x = AMotionEvent_getX(event, 0);
		engine->state.y = AMotionEvent_getY(event, 0);
		return 1;
	}
	return 0;
}

/**
* Traitement de la commande principale suivante.
*/
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	struct engine* engine = (struct engine*)app->userData;
	switch (cmd) {
	case APP_CMD_SAVE_STATE:
		// Le système demande d'enregistrer l'état actuel. Cette opération est effectuée.
		engine->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)engine->app->savedState) = engine->state;
		engine->app->savedStateSize = sizeof(struct saved_state);
		break;
	case APP_CMD_INIT_WINDOW:
		// La fenêtre est affichée : opération de préparation.
		if (engine->app->window != NULL) {
			engine_init_display(engine);
			engine_draw_frame(engine);
		}
		break;
	case APP_CMD_TERM_WINDOW:
		// La fenêtre est masquée ou fermée : opération de nettoyage.
		engine_term_display(engine);
		break;
	case APP_CMD_GAINED_FOCUS:
		// Quand l'application obtient le focus, la surveillance de l'accéléromètre est démarrée.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_enableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
			// L'objectif est de 60 événements par seconde.
			ASensorEventQueue_setEventRate(engine->sensorEventQueue,
				engine->accelerometerSensor, (1000L / 60) * 1000);
		}
		break;
	case APP_CMD_LOST_FOCUS:
		// Quand l'application perd le focus, la surveillance de l'accéléromètre est arrêtée.
		// Cela évite de décharger la batterie quand elle n'est pas utilisée.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_disableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
		}
		// Arrêt également de l'animation.
		engine->animating = 0;
		engine_draw_frame(engine);
		break;
	}
}

/**
* Il s'agit du point d'entrée principal d'une application native qui utilise
* android_native_app_glue. Elle s'exécute dans son propre thread, avec sa propre boucle d'événements
* pour recevoir des événements d'entrée et effectuer d'autres actions.
*/
void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	// Préparation de la surveillance de l'accéléromètre
	engine.sensorManager = ASensorManager_getInstance();
	engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
		ASENSOR_TYPE_ACCELEROMETER);
	engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
		state->looper, LOOPER_ID_USER, NULL, NULL);

	if (state->savedState != NULL) {
		// Un état enregistré précédent est utilisé pour procéder à la restauration.
		engine.state = *(struct saved_state*)state->savedState;
	}

	engine.animating = 1;

	// Boucle utilisée en attente de tâches à effectuer.

	while (1) {
		// Lecture de tous les événements en attente.
		int ident;
		int events;
		struct android_poll_source* source;

		// Si aucune animation n'a lieu, l'attente d'événements est bloquée indéfiniment.
		// En cas d'animation, la boucle est répétée jusqu'à ce que tous les événements soient lus, puis
		// la prochaine image d'animation est dessinée.
		while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {

			// Traitement de cet événement.
			if (source != NULL) {
				source->process(state, source);
			}

			// Traitement d'un capteur s'il possède des données.
			if (ident == LOOPER_ID_USER) {
				if (engine.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
						&event, 1) > 0) {
						LOGI("accelerometer: x=%f y=%f z=%f",
							event.acceleration.x, event.acceleration.y,
							event.acceleration.z);
					}
				}
			}

			// Vérification de la procédure de sortie.
			if (state->destroyRequested != 0) {
				engine_term_display(&engine);
				return;
			}
		}

		if (engine.animating) {
			// Événements terminés ; dessin de la prochaine image d'animation suivante.
			engine.state.angle += .01f;
			if (engine.state.angle > 1) {
				engine.state.angle = 0;
			}

			// Le dessin étant limité à la fréquence de mise à jour de l'écran,
			// aucune temporisation n'est nécessaire.
			engine_draw_frame(&engine);
		}
	}
}
