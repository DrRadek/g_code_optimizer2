from . import g_code_optimizer2_extension

def getMetaData():
    return {}

def register(app):
    return {"extension": g_code_optimizer2_extension.GCodeOptimizer2Extension()}
