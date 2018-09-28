#define MOVEMENTS (sizeof(movements)/sizeof(movements[0]))

int movements[][3] = {
  {0, 42, 92},
  {1, 41, 91},
  {0, 40, 91},
  {0, 40, 92},
  {0, 40, 91},
  {0, 41, 92},
  {0, 40, 92},
  {1, 41, 92},
  {0, 41, 91},
  {0, 40, 92},
  {0, 41, 91},
  {0, 40, 92},
  {0, 41, 91},
  {0, 14, 92},
  {-40, -1, 91},
  {-40, -1, 91},
  {-39, -1, 92},
  {-40, -2, 90},
  {-40, -1, 91},
  {-40, 0, 91},
  {-39, -2, 92},
  {-40, -1, 92},
  {-41, 0, 91},
  {-41, 0, 91},
  {-40, 0, 91},
  {-41, 0, 91},
  {-42, 1, 90},
  {-41, 0, 90},
  {-41, 0, 91},
  {-41, 0, 90},
  {-41, 0, 91},
  {-40, 0, 91},
  {-41, 0, 90},
  {-41, 0, 90},
  {-41, -1, 91},
  {-41, 0, 91},
  {-40, -1, 89},
  {-41, 0, 90},
  {-41, -1, 91},
  {-40, -2, 91},
  {-40, 0, 91},
  {-40, 0, 90},
  {-40, 0, 90},
  {-40, 0, 91},
  {-40, 0, 91},
  {-40, 0, 90},
  {-41, -1, 91},
  {-41, 0, 91},
  {-40, -1, 91},
  {-41, -1, 90},
  {-40, -1, 90},
  {-40, -1, 92},
  {-40, -1, 92},
  {-40, -2, 91},
  {-40, -2, 90},
  {-41, 0, 90},
  {-40, 0, 91},
  {-40, -1, 91},
  {-41, -1, 92},
  {-40, -1, 90},
  {-41, 0, 92},
  {-40, -1, 89},
  {-40, 0, 90},
  {-40, -1, 91},
  {-40, -1, 89},
  {-40, -1, 91},
  {-40, -1, 91},
  {-40, -1, 90},
  {-40, -1, 91},
  {-40, 0, 91},
  {-40, 2, 91},
  {-53, 34, 77},
  {-52, 44, 70},
  {-51, 46, 73},
  {-14, 63, 79},
  {-14, 64, 77},
  {-14, 64, 76},
  {-16, 65, 77},
  {-15, 64, 77},
  {-16, 63, 76},
  {-16, 65, 75},
  {-16, 66, 73},
  {-17, 69, 71},
  {-19, 69, 71},
  {-19, 69, 70},
  {-19, 71, 69},
  {-19, 73, 66},
  {-20, 74, 66},
  {-23, 78, 63},
  {-53, 67, 50},
  {-88, 47, 18},
  {-99, 41, 19},
  {-92, 36, 4},
  {-74, 45, -15},
  {-82, 55, -25},
  {-78, 60, -22},
  {-75, 61, -25},
  {-75, 62, -22},
  {-74, 62, -25},
  {-74, 64, -25},
  {-72, 66, -24},
  {-72, 67, -25},
  {-70, 69, -23},
  {-68, 69, -26},
  {-71, 66, -25},
  {-68, 70, -29},
  {-67, 69, -26},
  {-70, 67, -21},
  {-73, 65, -21},
  {-74, 62, -26},
  {-78, 58, -24},
  {-75, 61, -27},
  {-76, 62, -25},
  {-94, 76, 10},
  {-76, 78, -23},
  {-67, 74, 41},
  {-66, 67, 45},
  {-107, 22, 87},
  {-46, 92, -24},
  {-75, 119, -4},
  {-74, 25, 63},
  {-108, 31, 22},
  {-89, 50, -14},
  {-93, 31, -1},
  {-111, 11, -19},
  {-90, -30, -26},
  {-81, -38, -40},
  {-94, -21, -27},
  {-93, -18, -26},
  {-95, -21, -25},
  {-97, -20, -17},
  {-102, 0, -19},
  {-103, 47, -40},
  {-65, 82, 8},
  {-60, 67, -21},
  {-75, 84, 4},
  {-80, 60, -3},
  {-59, 42, -14},
  {-114, 92, 2},
  {-84, 87, 8},
  {-91, 76, -4},
  {-53, 73, 3},
  {-77, 74, -4},
  {-63, 57, 0},
  {-86, 54, 17},
  {-80, 58, 28},
  {-77, 74, 40},
  {-108, 81, 41},
  {-85, 50, 21},
  {-62, 61, 25},
  {-73, 59, 9},
  {-66, 78, 27},
  {-60, 47, 17},
  {-62, 68, 32},
  {-76, 80, 16},
  {-41, 65, 22},
  {-67, 84, 9},
  {-44, 77, 11},
  {-55, 74, 20},
  {-41, 74, 1},
  {-63, 107, 13},
  {-104, -2, 15},
  {-87, -49, 61},
  {-23, -7, 88},
  {-21, -23, 71},
  {0, -9, 97},
  {0, 4, 99},
  {0, 4, 100},
  {0, 4, 100},
  {1, 4, 100},
  {0, 4, 100},
  {0, 4, 100},
  {0, 4, 100},
  {1, 5, 100},
  {1, 4, 99}
};
