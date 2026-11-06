# PDNAM

Basic [NAM](https://www.neuralampmodeler.com/) loader externals for Pure Data. These externals are deliberately minimal with no additional options. The only argument is a relative path to a model file to load.

`pdnam~` uses the [MicroNAM](https://github.com/jaffco/MicroNAM) implementation and supports the four WaveNet model types: Nano, Feather, Lite, and Standard. Plus A2 lite and full models. By default it will pick full if available. By passing `lite` as second argument the A2 Lite model can be selected.

`pdnam+~` uses the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) implementation and supports both the optimized WaveNet and LSTM architectures. It also supports loading A2 models.

Several example models are included for testing.

## License

This project is released under the MIT license. As are MicroNAM and NeuralAudio.

"BossWN" WaveNet models come from NeuralAudio and are under MIT.

The "tw40_blues_deluxe_deerinkstudios.json" and "Tomato_Preamp_Maximum_Gain.aidax" models are under the CC BY-NC-ND 4.0 license.
