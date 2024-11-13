phytium-d2000.dtb: pd2008-devboard-dsk.dts pd2008-generic-psci-soc.dtsi
	cpp -nostdinc -I. -undef -x assembler-with-cpp pd2008-devboard-dsk.dts > tmp.dts
	dtc -O dtb -o phytium-d2000.dtb tmp.dts
	rm tmp.dts -rf
