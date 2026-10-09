## Notes on using the Optical Dial, results from the Dial Calibrator.
  RotaryDial::setDebounce(54);   // piso: filtra ruido e pulsos da volta
  RotaryDial::setMaxGap(0);       // teto: 0 = sem teto (recomendado)
// The gaps and separations can be within the same range.
RotaryDial::setDigitEnd(6752);  // Timeout: The digit is not entered after silence