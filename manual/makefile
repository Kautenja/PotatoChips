
all: manual

build:
	mkdir -p build

mNameCorpOctalWaveGenerator: build
	make -C NameCorpOctalWaveGenerator
	cp NameCorpOctalWaveGenerator/build/manual.pdf build/NameCorpOctalWaveGenerator.pdf

mInfiniteStairs: build
	make -C InfiniteStairs
	cp InfiniteStairs/build/manual.pdf build/InfiniteStairs.pdf

mJairasullator: build
	make -C Jairasullator
	cp Jairasullator/build/manual.pdf build/Jairasullator.pdf

mBossFight: build
	make -C BossFight
	cp BossFight/build/manual.pdf build/BossFight.pdf

mPulses: build
	make -C Pulses
	cp Pulses/build/manual.pdf build/Pulses.pdf

mPalletTownWavesSystem: build
	make -C PalletTownWavesSystem
	cp PalletTownWavesSystem/build/manual.pdf build/PalletTownWavesSystem.pdf

mMegaTone: build
	make -C MegaTone
	cp MegaTone/build/manual.pdf build/MegaTone.pdf

mPotKeys: build
	make -C PotKeys
	cp PotKeys/build/manual.pdf build/PotKeys.pdf

mSCC: build
	make -C SCC
	cp SCC/build/manual.pdf build/SCC.pdf

mTurboGrafx16: build
	make -C TurboGrafx16
	cp TurboGrafx16/build/manual.pdf build/TurboGrafx16.pdf

mStepSaw: build
	make -C StepSaw
	cp StepSaw/build/manual.pdf build/StepSaw.pdf

mSSMPEcho: build
	make -C S_SMP_Echo
	cp S_SMP_Echo/build/manual.pdf build/S_SMP_Echo.pdf

mSSMPGauss: build
	make -C S_SMP_Gauss
	cp S_SMP_Gauss/build/manual.pdf build/S_SMP_Gauss.pdf

manual: mNameCorpOctalWaveGenerator mInfiniteStairs mJairasullator mBossFight mPulses mPalletTownWavesSystem mMegaTone mPotKeys mSCC mTurboGrafx16 mStepSaw mSSMPEcho mSSMPGauss
