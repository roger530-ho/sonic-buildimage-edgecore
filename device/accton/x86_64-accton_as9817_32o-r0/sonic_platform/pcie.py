#############################################################################
# Edgecore
#
# Module contains an implementation of SONiC Platform Base API and
# provides the fan status which are available in the platform
# Base PCIe class
#############################################################################

import syslog
import os
try:
    from sonic_platform_base.sonic_pcie.pcie_common import PcieUtil
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")

class Pcie(PcieUtil):
    """Edgecore Platform-specific PCIe class"""

    def __init__(self, platform_path):
        PcieUtil.__init__(self, platform_path)
        self._conf_rev = self.__get_conf_rev()

    def __log(self, level, message):
        """
        Logs a message with the specified level, adding filename and class name as prefix.

        Logging behavior (e.g., log facility, identifier, destination, format) depends on the 
        system's syslog configuration and the environment of the caller using this class.

        Args:
            level (int): syslog log level (e.g., syslog.LOG_WARNING, syslog.LOG_ERR)
            message (str): The log message to record.
        """
        filename = os.path.basename(__file__)
        class_name = self.__class__.__name__
        syslog.syslog(level, f"{filename}:{class_name} {message}")

    def __get_conf_rev(self):
        """
        Retrieves the EEPROM label revision and finds the matching pcie.yaml file.

        Uses the Tlv object to get the label revision (0x27) and checks for the
        corresponding pcie.yaml file. Returns the revision if found.

        Returns:
            str: The label revision if a matching pcie.yaml file is found.
            None: If the label revision is not found or any error occurs.
        """
        try:
            import os
            from sonic_platform.eeprom import Tlv

            eeprom = Tlv()
            if eeprom is None:
                self.__log(syslog.LOG_WARNING, "Initializing the EEPROM object has failed.")
                return None

            # Try to get the TLV field for the label revision
            label_rev = eeprom._eeprom.get('0x27', None)
            if label_rev is None:
                self.__log(syslog.LOG_WARNING, "Cannot retrieve Label Revision (0x27) from the system EEPROM.")
                return None

            for rev in (label_rev, label_rev[:-1]):
                pcie_yaml_file = os.path.join(self.config_path, f"pcie_{rev}.yaml")
                if os.path.exists(pcie_yaml_file):
                    return rev

        except Exception as e:
            self.__log(syslog.LOG_WARNING, f"{str(e)}")
            pass

        return None
