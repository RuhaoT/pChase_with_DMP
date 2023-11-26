/*******************************************************************************
 * Copyright (c) 2006 International Business Machines Corporation.             *
 * All rights reserved. This program and the accompanying materials            *
 * are made available under the terms of the Common Public License v1.0        *
 * which accompanies this distribution, and is available at                    *
 * http://www.opensource.org/licenses/cpl1.0.php                               *
 *                                                                             *
 * Contributors:                                                               *
 *    Douglas M. Pase - initial API and implementation                         *
 *    Tim Besard - prefetching, JIT compilation                                *
 *******************************************************************************/

//
// Configuration
//

// Include guard
#if !defined(TIMER_H)
#define TIMER_H

// Local includes
#include "types.h"


//
// Class definition
//

class Timer {
public:
	static double seconds();
	static double resolution();
	static int64 ticks();
	static void calibrate();
	static void calibrate(int n);
private:
};

#endif
