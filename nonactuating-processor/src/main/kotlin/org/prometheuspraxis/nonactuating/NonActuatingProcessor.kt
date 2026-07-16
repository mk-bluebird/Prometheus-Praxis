package org.prometheuspraxis.nonactuating

import com.google.devtools.ksp.processing.*
import com.google.devtools.ksp.symbol.*
import java.io.InputStream
import java.util.Properties

data class ForbiddenPolicy(
    val forbiddenClasses: Set<String>,
    val forbiddenPackages: Set<String>,
    val forbiddenMethods: Set<String>
)

class NonActuatingProcessor(
    private val environment: SymbolProcessorEnvironment
) : SymbolProcessor {

    private val logger: KSPLogger = environment.logger
    private lateinit var policy: ForbiddenPolicy

    override fun init(options: Map<String, String>) {
        policy = loadPolicy(options)
    }

    private fun loadPolicy(options: Map<String, String>): ForbiddenPolicy {
        val policyPath = options["nonActuatingPolicy"] ?: "config/non_actuating_workload_policy.json"
        val file = java.io.File(policyPath)
        if (!file.exists()) {
            logger.error("Non-actuating policy file not found: $policyPath")
            return ForbiddenPolicy(emptySet(), emptySet(), emptySet())
        }

        val json = file.readText()
        val parser = com.fasterxml.jackson.module.kotlin.jacksonObjectMapper()
        val root = parser.readTree(json)

        val classes = mutableSetOf<String>()
        val packages = mutableSetOf<String>()
        val methods = mutableSetOf<String>()

        root.get("forbiddenClasses")?.forEach { classes.add(it.asText()) }
        root.get("forbiddenPackages")?.forEach { packages.add(it.asText()) }
        root.get("forbiddenMethods")?.forEach { methods.add(it.asText()) }

        return ForbiddenPolicy(
            forbiddenClasses = classes,
            forbiddenPackages = packages,
            forbiddenMethods = methods
        )
    }

    override fun process(resolver: Resolver): List<KSAnnotated> {
        val nonActuatingAnnotation = resolver.getClassDeclarationByName(
            resolver.getKSNameFromString("org.prometheuspraxis.nonactuating.NonActuatingWorkload")
        ) ?: return emptyList()

        val symbols = resolver.getSymbolsWithAnnotation(nonActuatingAnnotation.qualifiedName!!.asString())

        symbols.forEach { symbol ->
            when (symbol) {
                is KSClassDeclaration -> checkClass(symbol)
                is KSFile -> checkFile(symbol)
            }
        }

        return emptyList()
    }

    private fun checkFile(file: KSFile) {
        file.declarations
            .filterIsInstance<KSClassDeclaration>()
            .forEach { checkClass(it) }
    }

    private fun checkClass(cls: KSClassDeclaration) {
        val packageName = cls.packageName.asString()
        val className = cls.qualifiedName?.asString() ?: return

        if (policy.forbiddenPackages.contains(packageName)) {
            logger.error(
                "Non-actuating workload class $className resides in forbidden package $packageName",
                cls
            )
        }

        cls.getAllFunctions().forEach { fn ->
            checkFunction(fn, className)
        }
    }

    private fun checkFunction(fn: KSFunctionDeclaration, ownerClass: String) {
        fn.body?.accept(ForbiddenVisitor(logger, policy, ownerClass, fn), Unit)
    }

    private class ForbiddenVisitor(
        private val logger: KSPLogger,
        private val policy: ForbiddenPolicy,
        private val ownerClass: String,
        private val fn: KSFunctionDeclaration
    ) : KSVisitorVoid() {

        override fun visitCallExpression(expression: KSCallExpression, data: Unit) {
            val symbol = expression.reference.resolve()
            val qualifiedName = when (symbol) {
                is KSFunctionDeclaration -> symbol.qualifiedName?.asString()
                is KSPropertyDeclaration -> symbol.qualifiedName?.asString()
                else -> null
            }

            if (qualifiedName != null) {
                val pkg = symbol?.packageName?.asString() ?: ""

                if (policy.forbiddenMethods.contains(qualifiedName) ||
                    policy.forbiddenClasses.contains(ownerClass) ||
                    policy.forbiddenPackages.contains(pkg)
                ) {
                    logger.error(
                        "Non-actuating workload $ownerClass invokes forbidden actuator API: $qualifiedName",
                        fn
                    )
                }
            }

            super.visitCallExpression(expression, data)
        }
    }
}

class NonActuatingProcessorProvider : SymbolProcessorProvider {
    override fun create(environment: SymbolProcessorEnvironment): SymbolProcessor {
        return NonActuatingProcessor(environment)
    }
}
