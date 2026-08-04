// File: java/src/main/java/org/cyboquatic/forecast/HexLstSeq2Seq.java
package org.cyboquatic.forecast;

import org.deeplearning4j.nn.api.OptimizationAlgorithm;
import org.deeplearning4j.nn.conf.NeuralNetConfiguration;
import org.deeplearning4j.nn.conf.layers.GravesLSTM;
import org.deeplearning4j.nn.conf.layers.RnnOutputLayer;
import org.deeplearning4j.nn.conf.layers.Layer;
import org.deeplearning4j.nn.conf.GradientNormalization;
import org.deeplearning4j.nn.conf.inputs.InputType;
import org.deeplearning4j.nn.conf.MultiLayerConfiguration;
import org.deeplearning4j.nn.multilayer.MultiLayerNetwork;
import org.nd4j.linalg.activations.Activation;
import org.nd4j.linalg.lossfunctions.LossFunctions;
import org.nd4j.linalg.api.ndarray.INDArray;

/**
 * Seq2Seq-style time-series forecasting of hex-scale LST using DL4J,
 * without digital-twin classification.
 *
 * Inputs per time step (per hex-cell):
 *  - Telemetry variables:
 *      * cyboquatic workload features: energyreqJ, deltaVt_m_s, ker_e, ker_r.
 *      * PFAS concentration, DO level (if available).
 *  - Meteorological variables:
 *      * air temperature, humidity, wind speed, cloud cover.
 *  - Static/slow-varying variables:
 *      * day_of_year (encoded as sin/cos), soil moisture, canopy cover.
 *
 * The model predicts LST residuals or absolute LST at 24 hours ahead.
 *
 * Loss function penalising over-cooling (wasting energy):
 *  - Let y_true = target LST at horizon, y_pred = model prediction.
 *  - Define asymmetric loss:
 *        L = α * max(0, y_true - y_pred)^2    (under-prediction: too hot)
 *          + β * max(0, y_pred - y_true)^2   (over-prediction: over-cooling),
 *    with β > α to penalise over-cooling more strongly.
 *
 * Minimum sequence length:
 *  - For 24-hour ahead forecast with hourly data, a sequence length of at least
 *    48–72 time steps (2–3 days) captures diurnal cycles and short-term trends
 *    in both telemetry and meteorological variables, providing reliable context.
 *  - Shorter sequences (<24 steps) typically under-represent daily structure,
 *    degrading forecast accuracy and increasing energy waste.
 */
public final class HexLstSeq2Seq {

    private HexLstSeq2Seq() {}

    public static MultiLayerNetwork buildModel(int inputSize, int lstmUnits, int outputSize) {
        MultiLayerConfiguration conf = new NeuralNetConfiguration.Builder()
            .optimizationAlgo(OptimizationAlgorithm.STOCHASTIC_GRADIENT_DESCENT)
            .gradientNormalization(GradientNormalization.ClipElementWiseAbsoluteValue)
            .gradientNormalizationThreshold(1.0)
            .updater(new org.nd4j.linalg.learning.config.Adam(1e-3))
            .list()
            .layer(0, new GravesLSTM.Builder()
                .nIn(inputSize)
                .nOut(lstmUnits)
                .activation(Activation.TANH)
                .build())
            // Attention can be approximated by a second LSTM layer or custom attention;
            // for simplicity, we use a stacked LSTM here.
            .layer(1, new GravesLSTM.Builder()
                .nIn(lstmUnits)
                .nOut(lstmUnits)
                .activation(Activation.TANH)
                .build())
            .layer(2, new RnnOutputLayer.Builder()
                .nIn(lstmUnits)
                .nOut(outputSize) // e.g., 1 for LST residual
                .activation(Activation.IDENTITY)
                // Custom asymmetric loss implemented externally; here use MSE placeholder.
                .lossFunction(LossFunctions.LossFunction.MSE)
                .build())
            .setInputType(InputType.recurrent(inputSize))
            .build();

        MultiLayerNetwork net = new MultiLayerNetwork(conf);
        net.init();
        return net;
    }

    /**
     * Example asymmetric loss computation for over-cooling penalty.
     */
    public static double asymmetricLoss(double yTrue, double yPred, double alpha, double beta) {
        double under = Math.max(0.0, yTrue - yPred);
        double over  = Math.max(0.0, yPred - yTrue);
        return alpha * under * under + beta * over * over;
    }

    /**
     * Minimum sequence length recommendation for reliable 24-hour ahead forecast.
     *
     * @param stepSeconds telemetry/meteorological sampling interval in seconds.
     * @return minimum sequence length (number of time steps).
     */
    public static int recommendedSequenceLength(double stepSeconds) {
        double hoursPerStep = stepSeconds / 3600.0;
        // Target: cover at least 2–3 days of history.
        double targetHours = 72.0; // 3 days
        int seqLen = (int) Math.ceil(targetHours / hoursPerStep);
        return seqLen;
    }

    public static void main(String[] args) {
        int inputSize = 10;  // example: telemetry + meteo + static features
        int lstmUnits = 64;
        int outputSize = 1;

        MultiLayerNetwork net = buildModel(inputSize, lstmUnits, outputSize);
        System.out.println("Seq2Seq LST forecasting model initialised.");

        double stepSeconds = 3600.0; // hourly data
        int seqLen = recommendedSequenceLength(stepSeconds);
        System.out.println("Recommended sequence length for 24h ahead forecast: " + seqLen + " time steps.");

        double yTrue = 310.0; // K
        double yPred = 308.0; // K
        double loss = asymmetricLoss(yTrue, yPred, 1.0, 2.0);
        System.out.println("Example asymmetric loss (over-cooling penalised): " + loss);
    }
}
