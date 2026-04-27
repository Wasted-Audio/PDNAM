# PDNAM

Basic [NAM](https://www.neuralampmodeler.com/) loader externals for Pure Data. These externals are deliberatily minimal with no additional options. The only argument is a relative path to a model file to load.

`pdnam~` uses the [MicroNAM](https://github.com/jaffco/MicroNAM) implementation and supports the four WaveNet model types: Nano, Feather, Lite, and Standard.

`pdnam+~` uses the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) implementation and supports both the optimized WaveNet and LSTM architectures.

Several example models are included for testing.

## License

This project is released under the MIT license. As are MicroNAM and NeuralAudio.

"BossWN" WaveNet models come from NeuralAudio and are under MIT.

The "tw40_blues_deluxe_deerinkstudios.json" and "Tomato_Preamp_Maximum_Gain.aidax" models are under the CC BY-NC-ND 4.0 license.
