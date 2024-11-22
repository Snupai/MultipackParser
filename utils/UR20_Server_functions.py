# implementation of UR20 functions to be called by the server

# change active pallet
def UR20_SetActivePallet(pallet_number):
    '''
    pallet_number: number of pallet
    
    returns: 
        1 if pallet was set
        0 if pallet was not set
    '''
    return

# get active pallet number
def UR20_GetActivePalletNumber():
    '''
    returns: current number of active pallet
    '''
    return

# get pallet status
def UR20_GetPalletStatus(pallet_number):
    '''
    pallet_number: number of pallet

    returns: 
        1 if pallet is empty
        0 if pallet is full
    '''
    return