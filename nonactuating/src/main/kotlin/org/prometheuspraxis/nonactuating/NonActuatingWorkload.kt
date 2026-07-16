package org.prometheuspraxis.nonactuating

/**
 * Marks a class or file as non-actuating. Any use of forbidden
 * actuator APIs within the annotated scope will cause the build to fail.
 */
@Target(AnnotationTarget.CLASS, AnnotationTarget.FILE)
@Retention(AnnotationRetention.SOURCE)
annotation class NonActuatingWorkload
