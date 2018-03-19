#include <vector>
#include "BinaryProfile.h"
#include "PortableApi.h"

PROCESSOR_VENDOR GetProcessorVendor()
{
	int cpui[4];
	IntrinsicCpuid(cpui, 0, 0);
	char vendor[0x20];
	memset(vendor, 0, sizeof(vendor));
	*reinterpret_cast<int*>(vendor) = cpui[1];
	*reinterpret_cast<int*>(vendor + 4) = cpui[3];
	*reinterpret_cast<int*>(vendor + 8) = cpui[2];
	if (strcmp(vendor, "GenuineIntel") == 0)
	{
		return PROCESSOR_VENDOR_GENUINE_INTEL;
	}
	else if (strcmp(vendor, "AuthenticAMD") == 0)
	{
		return PROCESSOR_VENDOR_AUTHENTIC_AMD;
	}
	return PROCESSOR_VENDOR_UNKNOWN;
}

static void GetProcessorIdentificationInternal(BINARY_SECION_CPUID& pi, int leaf, int subleaf)
{
	CPUID_INFO cpuInfo;
	IntrinsicCpuid(cpuInfo.cpuInfo, leaf, subleaf);
	if (cpuInfo.cpuInfo[0] != 0 || cpuInfo.cpuInfo[1] != 0 ||
		cpuInfo.cpuInfo[2] != 0 || cpuInfo.cpuInfo[3] != 0)
	{
		cpuInfo.EAX = leaf;
		cpuInfo.ECX = subleaf;
		pi.cpuidInfo.push_back(cpuInfo);
	}
}

void GetProcessorIdentification(BINARY_SECION_CPUID& pi)
{
	CPUID_INFO cpuInfo;

	// Calling IntrinsicCpuId with 0x0 as the function_id argument
	// gets the number of the highest valid function ID.
	IntrinsicCpuid(cpuInfo.cpuInfo, 0, 0);
	int nIds = cpuInfo.cpuInfo[0];

	int subleaf = 0;

	for (int i = 0; i <= nIds; ++i)
	{
		switch (i)
		{
		case 0x04:
		{
			while (true)
			{
				IntrinsicCpuid(cpuInfo.cpuInfo, i, subleaf);
				if ((cpuInfo.cpuInfo[0] & 0xF) == 0)
					break; // Invalid cache type field.
				GetProcessorIdentificationInternal(pi, i, subleaf);
				++subleaf;
			}
			subleaf = 0;
			break;
		}
		case 0x07:
		{
			IntrinsicCpuid(cpuInfo.cpuInfo, i, subleaf);
			int maxSubleaf = cpuInfo.cpuInfo[0];
			for (int j = 0; j <= maxSubleaf; ++j)
			{
				GetProcessorIdentificationInternal(pi, i, j);
			}
			break;
		}
		case 0x0B:
		{
			while (true)
			{
				IntrinsicCpuid(cpuInfo.cpuInfo, i, subleaf);
				if ((cpuInfo.cpuInfo[2] & 0xFF00) == 0)
					break; // Invalid level type.
				GetProcessorIdentificationInternal(pi, i, subleaf);
				++subleaf;
			}
			subleaf = 0;
			break;
		}
		case 0x0D:
		{
			while (subleaf < 64)
			{
				GetProcessorIdentificationInternal(pi, i, subleaf);
				++subleaf;
			}
			subleaf = 0;
			break;
		}
		case 0x0F:
		case 0x10:
		{
			GetProcessorIdentificationInternal(pi, i, subleaf);
			GetProcessorIdentificationInternal(pi, i, subleaf + 1);
			break;
		}
		case 0x14:
		{
			IntrinsicCpuid(cpuInfo.cpuInfo, i, subleaf);
			int maxSubleaf = cpuInfo.cpuInfo[0];
			for (int j = 0; j < maxSubleaf; ++j)
			{
				GetProcessorIdentificationInternal(pi, i, j);
			}
			break;
		}
		default:
			GetProcessorIdentificationInternal(pi, i, subleaf);
			break;
		}
	}

	pi.nIds = (unsigned int)pi.cpuidInfo.size();

	// Calling IntrinsicCpuId with 0x80000000 as the function_id argument
	// gets the number of the highest valid extended ID.
	IntrinsicCpuid(cpuInfo.cpuInfo, 0x80000000, 0);
	int nExIds = cpuInfo.cpuInfo[0];

	for (int i = 0x80000000; i <= nExIds; ++i)
	{
		GetProcessorIdentificationInternal(pi, i, subleaf);
	}

	pi.nExIds = (unsigned int)pi.cpuidInfo.size() - pi.nIds;
}
