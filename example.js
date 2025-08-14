import loadLibsbmlsim from "./libsbmlsim.js"

const libsbmlsim = await loadLibsbmlsim();

const model =
`<?xml version="1.0" encoding="UTF-8"?>
<!-- Created by libAntimony version v2.15.0 with libSBML version 5.20.0. -->
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
  <model metaid="__main" id="__main">
    <listOfCompartments>
      <compartment sboTerm="SBO:0000410" id="default_compartment" spatialDimensions="3" size="1" constant="true"/>
    </listOfCompartments>
    <listOfSpecies>
      <species id="A" compartment="default_compartment" initialConcentration="10" hasOnlySubstanceUnits="false" boundaryCondition="false" constant="false"/>
      <species id="B" compartment="default_compartment" initialConcentration="0" hasOnlySubstanceUnits="false" boundaryCondition="false" constant="false"/>
      <species id="C" compartment="default_compartment" initialConcentration="0" hasOnlySubstanceUnits="false" boundaryCondition="false" constant="false"/>
    </listOfSpecies>
    <listOfParameters>
      <parameter id="k1" value="0.35" constant="true"/>
      <parameter id="k2" value="0.2" constant="true"/>
    </listOfParameters>
    <listOfReactions>
      <reaction id="_J0" reversible="true">
        <listOfReactants>
          <speciesReference species="A" stoichiometry="1" constant="true"/>
        </listOfReactants>
        <listOfProducts>
          <speciesReference species="B" stoichiometry="1" constant="true"/>
        </listOfProducts>
        <kineticLaw>
          <math xmlns="http://www.w3.org/1998/Math/MathML">
            <apply>
              <times/>
              <ci> k1 </ci>
              <ci> A </ci>
            </apply>
          </math>
        </kineticLaw>
      </reaction>
      <reaction id="_J1" reversible="true">
        <listOfReactants>
          <speciesReference species="B" stoichiometry="1" constant="true"/>
        </listOfReactants>
        <listOfProducts>
          <speciesReference species="C" stoichiometry="1" constant="true"/>
        </listOfProducts>
        <kineticLaw>
          <math xmlns="http://www.w3.org/1998/Math/MathML">
            <apply>
              <times/>
              <ci> k2 </ci>
              <ci> B </ci>
            </apply>
          </math>
        </kineticLaw>
      </reaction>
    </listOfReactions>
  </model>
</sbml>`

const simulator = new libsbmlsim.Simulator();
if (!simulator.LoadSbml(model)) {
    throw new Error(simulator.GetLastError());
}

const result = simulator.SimulateTimeCourse(10, 100);
if (!result) {
    throw new Error(simulator.GetLastError());
}

// print all the column names
const columnNames = [];
for (let i = 0; i < result.columns.size(); i++) {
    columnNames.push(result.columns.get(i).name);
}
console.log(columnNames.join(" "));

// print all column values
const rows = result.columns.get(0).values.size();
for (let i = 0; i < rows; i++) {
    const rowValues = [];
    for (let j = 0; j < result.columns.size(); j++) {
        rowValues.push(result.columns.get(j).values.get(i));
    }
    console.log(rowValues.join(" "));
}

// make sure to delete the result after use!
result.delete();