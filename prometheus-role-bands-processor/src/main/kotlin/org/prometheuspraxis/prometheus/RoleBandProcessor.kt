// File: prometheus-role-bands-processor/src/main/kotlin/org/prometheuspraxis/prometheus/RoleBandProcessor.kt
// Destination: Prometheus-Praxis/prometheus-role-bands/prometheus-role-bands-processor/src/main/kotlin/org/prometheuspraxis/prometheus/RoleBandProcessor.kt
// License: MIT OR Apache-2.0

package org.prometheuspraxis.prometheus

import java.io.BufferedReader
import java.io.InputStreamReader
import java.nio.charset.StandardCharsets
import javax.annotation.processing.AbstractProcessor
import javax.annotation.processing.Processor
import javax.annotation.processing.RoundEnvironment
import javax.annotation.processing.SupportedAnnotationTypes
import javax.annotation.processing.SupportedSourceVersion
import javax.lang.model.SourceVersion
import javax.lang.model.element.Element
import javax.lang.model.element.TypeElement
import javax.tools.Diagnostic

/**
 * Annotation used by Kotlin modules to declare their Prometheus role band.
 *
 * Example:
 *   @RoleBand("STEWARDSHIP_LOW")
 *   class StewardModule { ... }
 */
@Target(AnnotationTarget.CLASS, AnnotationTarget.FILE)
@Retention(AnnotationRetention.SOURCE)
annotation class RoleBand(val value: String)

/**
 * PrometheusRoleBands annotation processor.
 *
 * Responsibilities:
 *  - Load prometheus-role-bands.v1.aln and parse role band → forbidden crates mapping.
 *  - For each Kotlin element annotated with @RoleBand, discover its imports.
 *  - Emit compile-time errors if forbidden crates are imported in that module.
 */
@SupportedAnnotationTypes("org.prometheuspraxis.prometheus.RoleBand")
@SupportedSourceVersion(SourceVersion.RELEASE_11)
class RoleBandProcessor : AbstractProcessor() {

    private val roleBandConfig: RoleBandConfig by lazy {
        loadRoleBandConfig()
    }

    override fun process(
        annotations: MutableSet<out TypeElement>,
        roundEnv: RoundEnvironment
    ): Boolean {
        if (annotations.isEmpty()) {
            return false
        }

        val messager = processingEnv.messager

        for (annotation in annotations) {
            val annotatedElements = roundEnv.getElementsAnnotatedWith(annotation)
            for (element in annotatedElements) {
                val roleBand = extractRoleBandValue(element)
                if (roleBand == null) {
                    continue
                }

                val forbidden = roleBandConfig.forbiddenCrates[roleBand] ?: emptySet()

                if (forbidden.isEmpty()) {
                    continue
                }

                val imports = discoverImports(element)
                if (imports.isEmpty()) {
                    continue
                }

                val violations = forbidden.filter { forbiddenCrate ->
                    imports.any { imp -> imp.startsWith(forbiddenCrate) }
                }

                if (violations.isNotEmpty()) {
                    val msg = buildViolationMessage(roleBand, violations, imports)
                    messager.printMessage(Diagnostic.Kind.ERROR, msg, element)
                }
            }
        }

        return false
    }

    private fun buildViolationMessage(
        roleBand: String,
        violations: List<String>,
        imports: List<String>
    ): String {
        val sb = StringBuilder()
        sb.append("Role band '").append(roleBand)
            .append("' forbids importing crates: ").append(violations.joinToString(", "))
            .append(". Found imports: ").append(imports.joinToString(", "))
        return sb.toString()
    }

    private fun extractRoleBandValue(element: Element): String? {
        element.annotationMirrors.forEach { mirror ->
            val annotationType = mirror.annotationType
            val fqName = annotationType.toString()
            if (fqName == RoleBand::class.java.name) {
                val value = mirror.elementValues.entries
                    .firstOrNull { it.key.simpleName.toString() == "value" }
                    ?.value?.value as? String
                return value
            }
        }
        return null
    }

    /**
     * Discover imports for the given element.
     *
     * Due to annotation processing limitations, we do a best-effort:
     *  - Derive the source file path from the element.
     *  - Read the source and parse lines starting with "import ".
     */
    private fun discoverImports(element: Element): List<String> {
        val imports = mutableListOf<String>()

        val fileObject = processingEnv.elementUtils.getFileObjectOf(element) ?: return imports
        val inputStream = fileObject.openInputStream()
        BufferedReader(InputStreamReader(inputStream, StandardCharsets.UTF_8)).use { reader ->
            var line = reader.readLine()
            while (line != null) {
                val trimmed = line.trim()
                if (trimmed.startsWith("import ")) {
                    val importTarget = trimmed.removePrefix("import ").trim()
                    imports.add(importTarget)
                }
                line = reader.readLine()
            }
        }

        return imports
    }

    private fun loadRoleBandConfig(): RoleBandConfig {
        val resourceName = "/aln/prometheus-role-bands.v1.aln"
        val cl = RoleBandProcessor::class.java.classLoader
        val stream = cl.getResourceAsStream(resourceName)
            ?: return RoleBandConfig(emptyMap())

        val content = BufferedReader(InputStreamReader(stream, StandardCharsets.UTF_8))
            .use { it.readText() }

        return parseRoleBandConfig(content)
    }

    private fun BufferedReader.readText(): String {
        val sb = StringBuilder()
        var line = this.readLine()
        while (line != null) {
            sb.append(line).append("\n")
            line = this.readLine()
        }
        return sb.toString()
    }

    private fun parseRoleBandConfig(content: String): RoleBandConfig {
        val forbidden = mutableMapOf<String, MutableSet<String>>()

        for (block in content.split("role_band").drop(1)) {
            val bandIdMatch = Regex("""([A-Z0-9_]+)\s*{""").find(block)
            val bandId = bandIdMatch?.groupValues?.get(1) ?: continue

            val bandForbidden = forbidden.getOrPut(bandId) { mutableSetOf() }

            val forbidBlock = Regex("""forbidden_crates\s*=\s*\[(.*?)\]""", RegexOption.DOT_MATCHES_ALL)
                .find(block)?.groupValues?.get(1)

            if (forbidBlock != null) {
                Regex(""""(.*?)"""").findAll(forbidBlock).forEach { match ->
                    val crateName = match.groupValues[1].trim()
                    if (crateName.isNotEmpty()) {
                        bandForbidden.add(crateName)
                    }
                }
            }
        }

        return RoleBandConfig(forbidden)
    }
}

/**
 * Configuration derived from prometheus-role-bands.v1.aln:
 * role band → forbidden crate names.
 */
data class RoleBandConfig(
    val forbiddenCrates: Map<String, Set<String>>
)

/**
 * Utility to obtain a file object for an element.
 *
 * This relies on Javac internals; if unavailable, discoverImports will
 * simply return an empty list and no checks will be enforced.
 */
private fun javax.lang.model.util.Elements.getFileObjectOf(element: Element): javax.tools.FileObject? {
    return try {
        val javaElement = element
        val uriMethod = javaElement::class.java.getMethod("getSourcePosition")
        uriMethod.isAccessible = true
        val uri = uriMethod.invoke(javaElement) as? java.net.URI
        if (uri != null) {
            processingEnv.filer.getResource(javax.tools.StandardLocation.SOURCE_PATH, "", uri.path)
        } else {
            null
        }
    } catch (e: Exception) {
        null
    }
}
