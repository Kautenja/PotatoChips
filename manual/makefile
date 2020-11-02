
all: manual

build:
	mkdir -p build

m106: build
	make -C 106
	cp 106/build/manual.pdf build/106.pdf

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

mGBS: build
	make -C GBS
	cp GBS/build/manual.pdf build/GBS.pdf

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

mVRC6: build
	make -C VRC6
	cp VRC6/build/manual.pdf build/VRC6.pdf

mSSMPEcho: build
	make -C S_SMP_Echo
	cp S_SMP_Echo/build/manual.pdf build/S_SMP_Echo.pdf

mSSMPGauss: build
	make -C S_SMP_Gauss
	cp S_SMP_Gauss/build/manual.pdf build/S_SMP_Gauss.pdf

manual: m106 mTerracillator mJairasullator mBossFight mGleeokillator mGBS mMegaTone mPotillator mSCC mTurboGrafx16 mVRC6 mSSMPEcho mSSMPGauss
