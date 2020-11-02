
all: manual

build:
	mkdir -p build

mNameCorpOctalWaveGenerator: build
	make -C NameCorpOctalWaveGenerator
	cp NameCorpOctalWaveGenerator/build/manual.pdf build/NameCorpOctalWaveGenerator.pdf

mTerracillator: build
	make -C Terracillator
	cp Terracillator/build/manual.pdf build/Terracillator.pdf

mJairasullator: build
	make -C Jairasullator
	cp Jairasullator/build/manual.pdf build/Jairasullator.pdf

mBossFight: build
	make -C BossFight
	cp BossFight/build/manual.pdf build/BossFight.pdf

mGleeokillator: build
	make -C Gleeokillator
	cp Gleeokillator/build/manual.pdf build/Gleeokillator.pdf

mPalletTownWavesSystem: build
	make -C PalletTownWavesSystem
	cp PalletTownWavesSystem/build/manual.pdf build/PalletTownWavesSystem.pdf

mMegaTone: build
	make -C MegaTone
	cp MegaTone/build/manual.pdf build/MegaTone.pdf

mPotillator: build
	make -C Potillator
	cp Potillator/build/manual.pdf build/Potillator.pdf

mSCC: build
	make -C SCC
	cp SCC/build/manual.pdf build/SCC.pdf

mTurboGrafx16: build
	make -C TurboGrafx16
	cp TurboGrafx16/build/manual.pdf build/TurboGrafx16.pdf

mEscillator: build
	make -C Escillator
	cp Escillator/build/manual.pdf build/Escillator.pdf

mSSMPEcho: build
	make -C S_SMP_Echo
	cp S_SMP_Echo/build/manual.pdf build/S_SMP_Echo.pdf

mSSMPGauss: build
	make -C S_SMP_Gauss
	cp S_SMP_Gauss/build/manual.pdf build/S_SMP_Gauss.pdf

manual: mNameCorpOctalWaveGenerator mTerracillator mJairasullator mBossFight mGleeokillator mPalletTownWavesSystem mMegaTone mPotillator mSCC mTurboGrafx16 mEscillator mSSMPEcho mSSMPGauss
