import sys
import os
import json
import struct
import logging
import requests
from bluepy import btle


class EnbleBridge(btle.DefaultDelegate):

    def __init__(self, config_file_name, logger):
        btle.DefaultDelegate.__init__(self)
        with open(config_file_name) as config_file:
            self.config = json.load(config_file)
        self.validate_config()
        self.logger = logger


    def validate_config(self):
        """check if self.config has required config parameters"""

        required_parameters = {
            'sensor_name', 'server_address', 'server_port', 'post_url', 'access_token_set'}

        for param in required_parameters:
            if not param in set(self.config.keys()):
                raise Exception('Config does not contain ' + param)

        if len(self.config.get('access_token_set')) == 0:
            raise Exception('Config does not contain valid "access_token_set"')
        for access_token in self.config.get('access_token_set'):
            if not access_token.get('device_id'):
                raise Exception('Config does not contain valid "access_token_set"')
            if not access_token.get('access_token'):
                raise Exception('Config does not contain valid "access_token_set"')


    def handleDiscovery(self, dev, isNewDev, isNewData):
        """This is a delegate function for a btle scanner. 
            When a scanner receives BLE advertise packet, this function is called.
        """

        if not isNewData or not self.is_enable(dev):
            return

        try:
            manufacturer_data = dev.scanData.get(btle.ScanEntry.MANUFACTURER)
            measurement = self.parse_manufacturer_data(manufacturer_data)
            self.logger.debug('Get measurement data:' + str(measurement))
            self.send_measurement(measurement)

        except Exception as err:
            self.logger.error(err)


    def is_enable(self, dev):
        """Chech if a LocalName and manufacturer data in an advertise packet is appropriate as an ENBLE device."""

        sensor_name = self.config['sensor_name'].encode('utf-8')
        is_match_local_name = dev.scanData.get(btle.ScanEntry.COMPLETE_LOCAL_NAME) == sensor_name 
        manufacturer_data = dev.scanData.get(btle.ScanEntry.MANUFACTURER)
        if manufacturer_data:
            is_match_manufacturer_length = len(manufacturer_data) == 10
        else:
            is_match_manufacturer_length = False
        return is_match_local_name and is_match_manufacturer_length


    def parse_manufacturer_data(self, manufacturer_bin_data):
        """Parse manufacturer data in an advertise packet and get measured data."""

        len_of_data = len(manufacturer_bin_data)
        if len_of_data != 10:
            raise Exception("Length of manufacturer data must be 10, but it is " + str(len_of_data))
        
        unpacked_binary = struct.unpack('HHhHH', manufacturer_bin_data)
        measurement = {
            'device_id' : unpacked_binary[0],
            'battery' : float(unpacked_binary[1]) / 1000,
            'temperature' : float(unpacked_binary[2]) / 100,
            'humidity' : float(unpacked_binary[3]) / 10,
            'pressure' : float(unpacked_binary[4]) / 10
        }
        return measurement


    def get_access_token(self, device_id):
        """Find and get an access token corresponding with device_id."""

        # find an element in self.config['access_token_set'] whose device_id matches the argument of this function
        found = list(filter(lambda x : x.get('device_id') == device_id, self.config['access_token_set']))
        if len(found) == 0:
            raise Exception('The access token for device_id={} is not found'.format(device_id))
            
        access_token = found[0]['access_token']
        return access_token


    def get_post_url(self, device_id):
        """Get url for sending measured data to thingsboard server.
            ex) http://192.168.1.1:8080/api/v1/XXXACCESSTOKENXXX/telemetry
        """
        access_token = self.get_access_token(device_id)
        post_url = 'http://' + self.config['server_address'] + ':' + str(self.config['server_port']) + self.config['post_url']
        post_url = post_url.format(ACCESS_TOKEN=access_token)
        return post_url


    def send_measurement(self, measurement):
        """ Generate an appropreate url and post measured data."""
        device_id = measurement['device_id']
        post_url = self.get_post_url(device_id)
        server_response = requests.post(post_url, json=measurement)
        if not server_response.ok:
            raise Exception('Failed to post ThingsBoard server({0}):{1}'.format(post_url, server_response.status_code))



if __name__ == '__main__':
    logger = logging.getLogger()
    logger.setLevel(logging.WARN)

    stream_handler = logging.StreamHandler()    
    handler_format = logging.Formatter('[%(levelname)s] %(message)s')
    stream_handler.setFormatter(handler_format)
    logger.addHandler(stream_handler)

    try:
        path_to_this_file = os.path.dirname(os.path.abspath(__file__))
        enble_bridge = EnbleBridge(path_to_this_file + '/config.json', logger)
        scanner = btle.Scanner().withDelegate(enble_bridge)
    except Exception as err:
        logger.exception(err)
        sys.exit(1)

    print('start BLE scan.')

    while True:
        try:
            devices = scanner.scan(300)
        except btle.BTLEDisconnectError as err:
            # This exception offen occurs but this program works well.
            # So just log that the exception occure.
            logger.info(err)
        except btle.BTLEException as err:
            logger.exception(err)
            sys.exit(1)
        except KeyboardInterrupt as e:
            sys.exit(0)


