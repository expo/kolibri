package io.github.expo.kolibri.gradle

import org.gradle.api.model.ObjectFactory
import javax.inject.Inject

/**
 * Configuration entry point for the Kolibri Gradle plugin. Reserved for future options
 * (e.g. toggling diagnostics, code-gen modes); currently intentionally empty.
 */
open class KolibriGradleExtension @Inject constructor(objectFactory: ObjectFactory)
