lib.name = pdnam~

class.sources = pdnam~.cpp

datafiles = \
	pdnam~-help.pd \
	README.md \
	LICENSE

datadirs = data/

common.sources = \
	dep/NeuralAudio/NeuralAudio/NeuralModel.cpp \
	dep/NeuralAudio/NeuralAudio/RTNeuralLoader.cpp \
	dep/NeuralAudio/deps/RTNeural/RTNeural/RTNeural.cpp \
	dep/NeuralAudio/deps/NeuralAmpModelerCore/NAM/activations.cpp \
	dep/NeuralAudio/deps/NeuralAmpModelerCore/NAM/lstm.cpp \
	dep/NeuralAudio/deps/NeuralAmpModelerCore/NAM/dsp.cpp \
	dep/NeuralAudio/deps/NeuralAmpModelerCore/NAM/wavenet.cpp

# Definitions and flags
cflags = -std=gnu++20 -DNAM_SAMPLE_FLOAT -DDSP_SAMPLE_FLOAT \
	-DBUILD_INTERNAL_STATIC_WAVENET -DBUILD_INTERNAL_STATIC_LSTM \
	-DLSTM_MATH=FastMath -DWAVENET_MATH=FastMath \
	-DWAVENET_MAX_NUM_FRAMES=64 -DLAYER_ARRAY_BUFFER_PADDING=24 \
	-DRTNEURAL_NAMESPACE=RTNeural -DRTNEURAL_USE_EIGEN=1 -DRTNEURAL_DEFAULT_ALIGNMENT=16 \
	-Idep/NeuralAudio -Idep/NeuralAudio/NeuralAudio \
	-Idep/NeuralAudio/deps/RTNeural \
	-Idep/NeuralAudio/deps/math_approx/include \
	-Idep/NeuralAudio/deps/NeuralAmpModelerCore \
	-Idep/NeuralAudio/deps/RTNeural/modules/Eigen \
	-Idep/NeuralAudio/deps/RTNeural/modules/json \
	-Idep/NeuralAudio/deps/RTNeural-NAM \
	-Idep/NeuralAudio/deps/RTNeural-NAM/wavenet \
	-Idep/NeuralAudio/deps/RTNeural/modules/xsimd/include

define forDarwin
	cflags += -mmacosx-version-min=10.15
endef

PDLIBBUILDER_DIR=dep/pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
