
all: manual

build:
	mkdir -p build

m106: build
	make -C 106
	cp 106/build/manual.pdf build/106.pdf

m2612: build
	make -C 2612
	cp 2612/build/manual.pdf build/2612.pdf

m2A03: build
	make -C 2A03
	cp 2A03/build/manual.pdf build/2A03.pdf

mAY38910: build
	make -C AY-3-8910
	cp AY-3-8910/build/manual.pdf build/AY-3-8910.pdf

mFME7: build
	make -C FME7
	cp FME7/build/manual.pdf build/FME7.pdf

mGBS: build
	make -C GBS
	cp GBS/build/manual.pdf build/GBS.pdf

mPOKEY: build
	make -C POKEY
	cp POKEY/build/manual.pdf build/POKEY.pdf

mSCC: build
	make -C SCC
	cp SCC/build/manual.pdf build/SCC.pdf

mSN76489: build
	make -C SN76489
	cp SN76489/build/manual.pdf build/SN76489.pdf

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

manual: m106 m2612 m2A03 mAY38910 mFME7 mGBS mPOKEY mSCC mSN76489 mTurboGrafx16 mVRC6 mSSMPEcho mSSMPGauss
