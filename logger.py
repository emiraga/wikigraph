import logging
import conf

def setup_logger():
    # Setup logging
    logger = logging.getLogger('wikigraph')
    logger.setLevel(logging.DEBUG)
    # Use file output for production logging:
    filelog = logging.FileHandler(conf.LOGFILE, 'a')
    filelog.setLevel(logging.INFO)
    # Specify log formatting:
    formatter = logging.Formatter(
            "%(asctime)s %(name)s:%(lineno)s [%(levelname)s] %(message)s")
    filelog.setFormatter(formatter)
    # Add console log to logger
    if conf.DEBUG:
        # Use console for development logging:
        conlog = logging.StreamHandler()
        conlog.setLevel(logging.DEBUG)
        conlog.setFormatter(formatter)
        logger.addHandler(conlog)
    logger.addHandler(filelog)
        
    #start logging
    logger.info("Logger created")
    return logger

