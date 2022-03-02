def run(ocd, firmware_image):
    ocd.reset(halt=True)
    ocd.flash_write(firmware_image)
    ocd.resume()
    return True