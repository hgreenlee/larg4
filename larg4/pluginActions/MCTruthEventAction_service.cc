#include "larg4/pluginActions/MCTruthEventAction_service.h"
#include "art/Framework/Services/Registry/ServiceDefinitionMacros.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "artg4tk/services/ActionHolder_service.hh"
// Geant4  includes
#include "Geant4/G4Event.hh"
#include "Geant4/G4ParticleTable.hh"
#include "Geant4/G4IonTable.hh"
#include "Geant4/G4PrimaryVertex.hh"
#include "Geant4/G4ParticleDefinition.hh"
//
#include "art/Framework/Principal/Event.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nug4/G4Base/PrimaryParticleInformation.h"
#include <iostream>
#include <cmath>
#include <CLHEP/Vector/LorentzVector.h>
using std::string;

G4ParticleTable* larg4::MCTruthEventActionService::fParticleTable=nullptr;

larg4::MCTruthEventActionService::
MCTruthEventActionService(fhicl::ParameterSet const & p)
  : PrimaryGeneratorActionBase(p.get<string>("name", "MCTruthEventActionService"))
{}

larg4::MCTruthEventActionService::
~MCTruthEventActionService()
{
  // Print out a list of "unknown" PDG codes we saw in the input.
  if ( ! fUnknownPDG.empty() ){
    std::ostringstream badtxt;
    std::map<G4int, G4int>::iterator itr = fUnknownPDG.begin();
    for ( ; itr != fUnknownPDG.end(); ++itr ){
      int pdg = (*itr).first;
      badtxt << "\n   Unknown PDG code = " << pdg
             << ", seen " << (*itr).second << " times.";

      const int genieLo = 2000000001;
      const int genieHi = 2000000300;
      if ( pdg >= genieLo && pdg <= genieHi ) {
        badtxt << "  (GENIE specific)";
      }
    }

    mf::LogWarning("MCTruthEventAction")
      << "The following unknown PDG codes were present in the "
      << "simb::MCTruth input.\n"
      << "They were not processed by Geant4."
      << badtxt.str();
  }

  // Print out a list of PDG codes we saw in the input that weren't status=1
  if ( ! fNon1StatusPDG.empty() ){
    std::ostringstream non1txt;
    std::map<G4int, G4int>::iterator itr = fNon1StatusPDG.begin();
    for ( ; itr != fNon1StatusPDG.end(); ++itr ){
      int pdg = (*itr).first;
      non1txt << "\n   PDG code = " << pdg
              << ", seen " << (*itr).second << " times.";

      const int genieLo = 2000000001;
      const int genieHi = 2000000300;
      if ( pdg >= genieLo && pdg <= genieHi ) {
        non1txt << "  (GENIE specific)";
      }
    }

    mf::LogDebug("MCTruthEventAction")
      << "The following PDG codes were present in the "
      << "simb::MCTruth input with Status != 1.\n"
      << "They were not processed by Geant4."
      << non1txt.str();
  }

  // Print out a list of PDG codes that were processed
  if ( ! fProcessedPDG.empty() ){
    std::ostringstream goodtxt;
    std::map<G4int, G4int>::iterator itr = fProcessedPDG.begin();
    for ( ; itr != fProcessedPDG.end(); ++itr ){
      int pdg = (*itr).first;
      goodtxt << "\n   PDG code = " << pdg
              << ", seen " << (*itr).second << " times.";
    }
    mf::LogDebug("MCTruthEventAction")
      << "The following PDG codes were present in the "
      << "simb::MCTruth input with Status = 1 "
      << "and were processed by Geant4"
      << goodtxt.str();
  }


}


// Create a primary particle for an event!
// (Standard Art G4 simulation)
void larg4::MCTruthEventActionService::generatePrimaries(G4Event * anEvent) {
  // For each MCTruth (probably only one, but you never know):
  // index keeps track of which MCTruth object you are using
  size_t index = 0;
  std::map< CLHEP::HepLorentzVector, G4PrimaryVertex* >                  vertexMap;
  std::map< CLHEP::HepLorentzVector, G4PrimaryVertex* >::const_iterator  vi;
  art::ServiceHandle<artg4tk::ActionHolderService> actionHolder;
  art::Event & evt = actionHolder -> getCurrArtEvent();
  auto const mclistHandles = evt.getMany<std::vector<simb::MCTruth>>();

  size_t mclSize = mclistHandles.size(); // -- should match the number of generators
  mf::LogDebug("generatePrimaries") << "MCTruth Handles Size: " << mclSize;

  // -- Loop over MCTruth Handle List
  for(size_t mcl = 0; mcl < mclSize; ++mcl)
  {
    mf::LogDebug("generatePrimaries") << "MCTruth Handle Number: " << (mcl+1) << " of " << mclSize;
    art::Handle< std::vector<simb::MCTruth> > mclistHandle = mclistHandles[mcl];
    // -- Loop over all MCTruth handle entries for a given generator, usually only one, but you never know
    for(size_t i = 0; i < mclistHandle->size(); ++i)
    {
      art::Ptr<simb::MCTruth> mclist(mclistHandle, i);

      mf::LogDebug("generatePrimaries") << "art::Ptr number " << (i+1) << " of "
                                        << mclistHandle->size() << ", Ptr: " << mclist;
      int nPart = mclist->NParticles();
      MF_LOG_INFO("generatePrimaries") << "Generating " << nPart << " particles" ;

      // -- Loop over all particles in MCTruth Object
      for(int m = 0; m != nPart; ++m)
      {
        simb::MCParticle const& particle = mclist->GetParticle(m);
        // Get the Particle Data Group code for the particle.
        G4int pdgCode = particle.PdgCode();

        if ( particle.StatusCode() != 1 ) {
          MF_LOG_DEBUG("generatePrimaries") << "Status code != 1, skipping particle number with MCTruth index = "
                                            << "(" << mcl << "," << i << ")" << " and particle index = " << m;
          fNon1StatusPDG[ pdgCode ] += 1;
          continue;
        }

        if( ((m+1)%nPart) < 2 ) // -- only first and last will satisfy this
        {
          mf::LogDebug("generatePrimaries") << "Particle Number:  " << (m+1) << " of " << nPart;
          mf::LogDebug("generatePrimaries") << "TrackID: " << particle.TrackId();
          mf::LogDebug("generatePrimaries") << "index: " << index;
        }

        G4double x = particle.Vx() * CLHEP::cm;
        G4double y = particle.Vy() * CLHEP::cm;
        G4double z = particle.Vz() * CLHEP::cm;
        G4double t = particle.T()  * CLHEP::ns;

        // Create a CLHEP four-vector from the particle's vertex.
        CLHEP::HepLorentzVector fourpos(x,y,z,t);

        // Is this vertex already in our map?
        G4PrimaryVertex* vertex = nullptr;
        auto result = vertexMap.find( fourpos );
        if ( result == vertexMap.end() ) {
          // No, it's not, so create a new vertex and add it to the
          // map.
          vertex = new G4PrimaryVertex(x, y, z, t);
          vertexMap[ fourpos ] = vertex;

          // Add the vertex to the G4Event.
          anEvent->AddPrimaryVertex( vertex );
        }
        else {
          // Yes, it is, so use the existing vertex.
          vertex = result->second;
        }

        // Get additional particle information.
        TLorentzVector momentum = particle.Momentum(); // (px,py,pz,E)
        TVector3 polarization = particle.Polarization();

        // Get the particle table if necessary.  (Note: we're
        // doing this "late" because I'm not sure at what point
        // the G4 particle table is initialized in the loading process.
        if (fParticleTable == nullptr) {
          fParticleTable = G4ParticleTable::GetParticleTable();
        }

        // Get Geant4's definition of the particle.
        G4ParticleDefinition* particleDefinition = nullptr;

        if (pdgCode == 0) {
          particleDefinition = fParticleTable->FindParticle("opticalphoton");
        }
        else {
          particleDefinition = fParticleTable->FindParticle(pdgCode);
        }

        if (pdgCode >= 2'000'000'000) { // If the particle is generator-specific
          mf::LogDebug("ConvertPrimaryToGeant4")
            << ": %%% Will skip particle with generator-specific PDG code = " << pdgCode;
          fUnknownPDG[ pdgCode ] += 1;
          continue;
        }
        if (pdgCode > 1'000'000'000) { // If the particle is a nucleus
          mf::LogDebug("ConvertPrimaryToGeant4") << ": %%% Nuclear PDG code = " << pdgCode
                                                 << " (x,y,z,t)=(" << x
                                                 << "," << y
                                                 << "," << z
                                                 << "," << t << ")"
                                                 << " P=" << momentum.P()
                                                 << ", E=" << momentum.E();
          // If the particle table doesn't have a definition yet, ask the ion
          // table for one. This will create a new ion definition as needed.
          if ( ! particleDefinition ) {
            int Z = (pdgCode % 10'000'000) / 10'000; // atomic number
            int A = (pdgCode % 10'000) / 10; // mass number
            particleDefinition = fParticleTable->GetIonTable()->GetIon(Z, A, 0.);
          }
        }

        // What if the PDG code is unknown?  This has been a known
        // issue with GENIE.
        if (particleDefinition == nullptr) {
          mf::LogDebug("ConvertPrimaryToGeant4") << ": %%% Code not found = " << pdgCode;
          fUnknownPDG[ pdgCode ] += 1;
          continue;
        }

        fProcessedPDG[ pdgCode ] += 1;
        // Create a Geant4 particle to add to the vertex.
        auto* g4particle = new G4PrimaryParticle(particleDefinition,
                                                 momentum.Px() * CLHEP::GeV,
                                                 momentum.Py() * CLHEP::GeV,
                                                 momentum.Pz() * CLHEP::GeV);

        // Add more particle information the Geant4 particle.
        g4particle->SetCharge(particleDefinition->GetPDGCharge());
        g4particle->SetPolarization(polarization.x(),
                                    polarization.y(),
                                    polarization.z());

        // Add the particle to the vertex.
        vertex->SetPrimary(g4particle);

        // Create a PrimaryParticleInformation object, and save
        // the MCTruth pointer in it.  This will allow the
        // ParticleActionList class to access MCTruth
        // information during Geant4's tracking.

        g4b::PrimaryParticleInformation* primaryParticleInfo = new g4b::PrimaryParticleInformation;
        primaryParticleInfo->SetMCTruth( mclist.get(), index, m );

        // Save the PrimaryParticleInformation in the
        // G4PrimaryParticle for access during tracking.
        g4particle->SetUserInformation( primaryParticleInfo );

      } // -- for each particle in MCTruth
      ++index;
    } // -- for each MCTruth entry
  } // -- for each MCTruth handle
}// -- generatePrimaries()

void larg4::MCTruthEventActionService::addG4Particle(G4Event *event,
                                                     int pdgId,
                                                     const G4ThreeVector& pos,
                                                     double time,
                                                     double energy,
                                                     const G4ThreeVector& mom)
{
  // Create a new vertex
  auto* vertex = new G4PrimaryVertex(pos, time);
  vertex->SetPrimary(new G4PrimaryParticle(pdgId,
                                           mom.x(),
                                           mom.y(),
                                           mom.z(),
                                           energy));
  event->AddPrimaryVertex(vertex);
}

DEFINE_ART_SERVICE(larg4::MCTruthEventActionService)
