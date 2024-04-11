#include "core.h"

#include <cassert>

Core::Core(numMac_t _mac_num, numMac_t _LR_mac_num, energy_t _LR_mac_cost):
	mac_num(_mac_num), LR_mac_num(_LR_mac_num), LR_mac_cost(_LR_mac_cost){}

PolarCore::PolarCore(const PESetting& _pes, numMac_t _LR_mac_num, energy_t _LR_mac_cost,
					 const Bus& _noc, const Buffers& _bufs):
	Core(_noc.aLen * _noc.oLen * _pes.laneNum * _pes.vecSize, _LR_mac_num, _LR_mac_cost),
	pes(_pes), bus(_noc),
	al1(_bufs.al1), wl1(_bufs.wl1), ol1(_bufs.ol1),
	al2(_bufs.al2), wl2(_bufs.wl2), ol2(_bufs.ol2),
	ul3(_bufs.ul3)
{
	assert(al2.Size == 0 || al2.Size >= al1.Size * bus.totNum);
	assert(wl2.Size == 0 || wl2.Size >= wl1.Size * bus.totNum * pes.laneNum);
}

const Core::Buffer& PolarCore::ubuf() const{
	return ul3;
}

PolarCore::PESetting::PESetting(vmac_t _vecSize, vmac_t _laneNum, energy_t _macCost):
	vecSize(_vecSize),laneNum(_laneNum),MACCost(_macCost){}

Core::Buffer::Buffer(vol_t _size, energy_t _rCost, bw_t _rBW, energy_t _wCost, bw_t _wBW):
	Size(_size), RCost(_rCost), WCost(_wCost <= 0?_rCost:_wCost),
	RBW(_rBW), WBW(_wBW<=0?_rBW:_wBW){}

PolarCore::Bus::Bus(numpe_t _aLen, numpe_t _oLen, energy_t _hopCost, bw_t _busBW):
	aLen(_aLen),oLen(_oLen),totNum(_aLen*_oLen),
	hopCost(_hopCost),busBW(_busBW){}

EyerissCore::EyerissCore(const PESetting& _pes, numMac_t _LR_mac_num, energy_t _LR_mac_cost,
						 const Buses& _buses, const Buffers& _bufs):
	Core(_pes.Xarray * _pes.Yarray, _LR_mac_num, _LR_mac_cost), pes(_pes),
	ibus(_buses.ibus), wbus(_buses.wbus), pbus(_buses.pbus),
	al1(_bufs.al1), wl1(_bufs.wl1), pl1(_bufs.pl1), ul2(_bufs.ul2) {}

const Core::Buffer& EyerissCore::ubuf() const{
	return ul2;
}

EyerissCore::PESetting::PESetting(vmac_t _Xarray, vmac_t _Yarray, energy_t _MacCost) :
	Xarray(_Xarray), Yarray(_Yarray), MacCost(_MacCost){}

EyerissCore::Bus::Bus(energy_t _BusCost, bw_t _BusBW) :
	BusCost(_BusCost), BusBW(_BusBW) {}
